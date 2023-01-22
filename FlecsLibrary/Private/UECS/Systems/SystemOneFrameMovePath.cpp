#include "UECS/Systems/SystemOneFrameMovePath.h"

#include "UECS/SystemTasks.h"
#include "UECS/Components/BaseComponents.h"

void SystemOneFrameMovePath::initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld)
{
	UnrealEcsSystem::initialize(inOwner, inEcsWorld);

	queryMoveSequence = inEcsWorld->query<FOneFrameMovementSequence, FPosition, FRotationComponent>();
}

void SystemOneFrameMovePath::schedule(UnrealEcsSystemScheduler* inScheduler)
{
	if(nullptr == ownerSubsys) { return; }
	
	SystemTaskChainBuilder builder("FollowPath", 302, inScheduler);
	builder.AddDependency("CalculateAvoidanceVelocity");

	// Task: move along path.
	{
		TaskDependencies deps;
		deps.AddRead<FOneFrameMovementSequence>();
		deps.AddWrite<FPosition>();
		deps.AddWrite<FRotationComponent>();
	
		builder.AddTask(deps, [=](flecs::world& world)
		{
			queryMoveSequence.iter([&](flecs::iter& iterator, FOneFrameMovementSequence* moveSequence, FPosition* position, FRotationComponent* rotation)
			{
				UWorld* unrealWorld = ownerSubsys->GetWorld();
				const float deltaTime = unrealWorld->GetDeltaSeconds();
				
				for(auto idx : iterator)
				{
					auto entity = iterator.entity(idx);
					const bool hasCollision = entity.has<FCollisionShapeComponent>();
					
					for(const auto step : moveSequence[idx].steps)
					{
						const FVector startPosition = position[idx].value;
						
						if(hasCollision)
						{
							auto collision = iterator.table_column<FCollisionShapeComponent>();
							const FCollisionShape shape		= collision[idx].shape;
							const ECollisionChannel channel	= collision[idx].collisionChannel;
							const FVector offset			= collision[idx].offset;
							const FVector endPosition		= startPosition + step + offset;

							// If we hit something, we abort our movement and raise a hit event.
							FHitResult hit;
							const bool hitSomething = unrealWorld->SweepSingleByChannel(hit, startPosition, endPosition, rotation[idx].value, channel, shape);
							if(hitSomething)
							{
								position[idx].value = hit.Location;
								break;
							}
						}
						
						position[idx].value = step;
					}

					// FOneFrameMovementSequence is only meant for one frame, not long-standing movements.
					// Use FMovementPath for multi-frame sequences.
					world.defer_begin();
						entity.remove<FOneFrameMovementSequence>();
					world.defer_end();
				}
			});
		}, ESysTaskType::GameThread);
	}

	inScheduler->AddSystemTaskChain(builder.FinishGraph());
}
