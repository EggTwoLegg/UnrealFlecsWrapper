#include "UECS/Systems/SystemFindNavmeshPath.h"

#include "NavigationSystem.h"
#include "UECS/SystemTasks.h"
#include "UECS/Components/BaseComponents.h"

void SystemFindNavmeshPath::initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld)
{
	UnrealEcsSystem::initialize(inOwner, inEcsWorld);

	pathCompletedCallback = FNavPathQueryDelegate::CreateLambda([&](uint32 requestId, ENavigationQueryResult::Type result, FNavPathSharedPtr pathPtr)
	{
		const auto* find = entityPathQueryIdMap.Find(requestId);
		if(nullptr == find) { return; }

		auto entity = *find;
		if(!entity.is_alive() || result != ENavigationQueryResult::Success) { return; }

		// Associated entity needs a nav mesh path and a movement sequence component to actually follow that path.
		inEcsWorld->defer_begin();
			FNavMeshPath path;
			path.path = pathPtr->GetPathPoints();
			entity.set<FNavMeshPath>(path);
			entity.set<FOneFrameMovementSequence>({});
		inEcsWorld->defer_end();
	});

	// Init queries.
	queryPathRequests   = ecsWorld->query<FNavmeshAgent, FNavmeshPathRequest>();
	queryAgentPositions = ecsWorld->query<FNavMeshPath, FOneFrameMovementSequence, FPosition, FSpeed, FVelocity>();
}

void SystemFindNavmeshPath::schedule(UnrealEcsSystemScheduler* inScheduler)
{
	UNavigationSystemV1* navSystem = Cast<UNavigationSystemV1>(ownerSubsys->GetWorld()->GetNavigationSystem());
	if(nullptr == navSystem) { return; }

	SystemTaskChainBuilder builder("FindNavmeshPath", 300, inScheduler);
	
	// Task: Request paths.
	{
		TaskDependencies deps;
		deps.AddRead<FNavmeshAgent>();
		deps.AddRead<FNavmeshPathRequest>();

		builder.AddTask(deps, [=](flecs::world& world)
		{
			queryPathRequests.iter([&](const flecs::iter& iterator, const FNavmeshAgent* agent, const FNavmeshPathRequest* request)
			{
				for(const auto idx : iterator)
				{
					auto entity = iterator.entity(idx);
					
					const FPathFindingQuery query(ownerSubsys, *navSystem->MainNavData.Get(), request[idx].startLocation, request[idx].endLocation);
					entityPathQueryIdMap.FindOrAdd(navSystem->FindPathAsync(agent[idx].agentProperties, query, pathCompletedCallback), entity);

					world.defer_begin();
						entity.remove<FNavmeshPathRequest>();
					world.defer_end();
				}
			});
		}, ESysTaskType::GameThread);
	}

	// Task: build movement sequence.
	{
		TaskDependencies deps;
		deps.AddRead<FSpeed>();
		deps.AddRead<FPosition>();
		deps.AddRead<FVelocity>();
		deps.AddWrite<FNavMeshPath>();
		deps.AddWrite<FOneFrameMovementSequence>();

		builder.AddTask(deps, [=](flecs::world& world)
		{
			queryAgentPositions.iter([&](const flecs::iter& iterator, FNavMeshPath* path,
				FOneFrameMovementSequence* movement, const FPosition* position, const FSpeed* speed, const FVelocity* velocity)
			{
				const float deltaTime = ownerSubsys->GetWorld()->GetDeltaSeconds();
				
				for(const auto idx : iterator)
				{
					// No path points? No movement.
					auto& pathPoints = path[idx].path;
					const int32 numPathPoints = pathPoints.Num();
					if(numPathPoints <= 0) { continue; }

					// Determine how much we can travel for this update step.
					const float maxSpeed     = speed[idx].max;
					const float acceleration = speed[idx].linearAcceleration * deltaTime;
					const float oldVelMag = velocity[idx].value.SquaredLength();
					const float newVelMag = FMath::Clamp(oldVelMag + acceleration * acceleration, 0.0f, maxSpeed * maxSpeed);
					float remainingDistSq = newVelMag * newVelMag * deltaTime;

					int32 idxPathPoint = path[idx].idxPoint;
					FVector agentPosition = position[idx].value;
					
					// Enqueue steps along the navmesh path.
					while(remainingDistSq > KINDA_SMALL_NUMBER && idxPathPoint < numPathPoints)
					{
						const FVector goalLocation= pathPoints[idxPathPoint];
						const float sqrDistToNextPoint = FVector::DistSquared(agentPosition, goalLocation);
						const FVector moveStep = (goalLocation - agentPosition) * (sqrDistToNextPoint / FMath::Min(remainingDistSq, sqrDistToNextPoint));

						agentPosition += moveStep;
						movement[idx].steps.Add(moveStep);
						remainingDistSq -= sqrDistToNextPoint;

						// If we've exhausted all of the segment, then increment the index. Otherwise, we're going to
						// have incorrect paths that jump ahead before we've reached waypoints.
						if(remainingDistSq >= 0.01f) { ++idxPathPoint; }
					}

					// Set path index so that the next cycle can continue where we left off.
					path[idx].idxPoint = idxPathPoint;

					// Remove the path from our entity if we've reached the end.
					if(idxPathPoint >= numPathPoints - 1)
					{
						auto entity = iterator.entity(idx);
						
						world.defer_begin();
							entity.remove<FNavMeshPath>();
						world.defer_end();
					}
				}
			});
		}, ESysTaskType::TaskGraph);
	}

	inScheduler->AddSystemTaskChain(builder.FinishGraph());
}
