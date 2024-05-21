#include "UECS/Systems/SystemFindNavmeshPath.h"

#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "UECS/DetourNavigation.h"
#include "UECS/Components/BaseComponents.h"
#include "UECS/Components/MovementInput.h"
#include "UECS/Components/NavmeshPathRequest.h"
#include "UECS/Components/NavmeshPathResponse.h"

SystemFindNavmeshPath::SystemFindNavmeshPath(flecs::world& World)
{
	QueryPathRequests   = new flecs::query(World.query<const FNavmeshAgent, const FNavmeshPathRequest>());
	QueryNavmeshInput   = new flecs::query(World.query<const FNavmeshAgent, const FPosition, const FMovementInput, const FSpeed, FOneFrameMovementSequence>());
	QueryEnqueueMovement = new flecs::query(World.query<FNavmeshPathResponse, FOneFrameMovementSequence, FPosition, FSpeed, FVelocity>());
}

void SystemFindNavmeshPath::Prep(int32 NumThreads)
{
	WorkerPathRequests.Reset(NumThreads);
	WorkerNavmeshInput.Reset(NumThreads);
	WorkerEnqueueMovement.Reset(NumThreads);
	for(int32 IdxThread = 0; IdxThread < NumThreads; ++IdxThread)
	{
		WorkerPathRequests.Add(QueryPathRequests->worker(IdxThread, NumThreads));
		WorkerNavmeshInput.Add(QueryNavmeshInput->worker(IdxThread, NumThreads));
		WorkerEnqueueMovement.Add(QueryEnqueueMovement->worker(IdxThread, NumThreads));
	}
}

void SystemFindNavmeshPath::Iter_FindPath(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const
{
	const UNavigationSystemV1* NavSystem = Cast<UNavigationSystemV1>(UnrealWorld->GetNavigationSystem());
	if(nullptr == NavSystem) { return; }

	ARecastNavMesh* NavData = Cast<ARecastNavMesh>(NavSystem->GetMainNavData());
	if(nullptr == NavData) { return; }

	dtNavMesh* NavMesh = NavData->GetRecastMesh();
	if(nullptr == NavMesh) { return; }

	WorkerPathRequests[IdxThread].each([NavMesh](const flecs::iter& Iterator, size_t IdxEntity, const FNavmeshAgent& Agent, const FNavmeshPathRequest& Request)
	{
		const FDetourNavigation DetourNavigation(NavMesh);
		
		FNavmeshPathResponse Response;
		Response.Status = DetourNavigation.FindPath(Request.Start, Request.End, Agent.Extents, Response.Points);

		if(Response.Status != DT_FAILURE)
		{
			flecs::entity Entity = Iterator.entity(IdxEntity);
			//Entity.set(Response);
		}
	});
}

void SystemFindNavmeshPath::Iter_EnqueueMovement(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const
{
	WorkerEnqueueMovement[IdxThread].each([DeltaTime](const flecs::iter& Iterator, size_t Idx, FNavmeshPathResponse& Path,
		FOneFrameMovementSequence& Movement, const FPosition& Position, const FSpeed& Speed, const FVelocity& Velocity)
	{
		// No path points? No movement.
		auto& PathPoints = Path.Points;
		const int32 NumPathPoints = PathPoints.Num();
		if(NumPathPoints <= 0)
		{
			flecs::entity Entity = Iterator.entity(Idx);
			Entity.remove<FNavmeshPathResponse>();
			return;
		}

		// Determine how much we can travel for this update step.
		const float MaxSpeed     = Speed.max;
		const float Acceleration = Speed.linearAcceleration * DeltaTime;
		const float OldVelMag = Velocity.Value.SquaredLength();
		const float NewVelMag = FMath::Clamp(OldVelMag + Acceleration * Acceleration, 0.0f, MaxSpeed * MaxSpeed);
		float RemainingDistSq = NewVelMag * NewVelMag * DeltaTime;
		
		int32 IdxPathPoint    = 0;
		FVector AgentPosition = Position.Value;

		// Perform a binary search on the path points to find the closest point to the agent.
		// This is necessary because the agent may not be at the exact point in the path.
		// This is also necessary because the agent may have moved since the last update.
		if(NumPathPoints > 1)
		{
			int32 Low = 0;
			int32 High = NumPathPoints - 1;
			
			while(Low < High)
			{
				const int32 Mid = (Low + High) / 2;
				const FVector& Point = PathPoints[Mid];
				const float SqrDist = FVector::DistSquared(AgentPosition, Point);
				if(SqrDist < RemainingDistSq)
				{
					High = Mid;
				}
				else
				{
					Low = Mid + 1;
				}
			}
			
			IdxPathPoint = Low;
		}
		
		// Enqueue steps along the navmesh path.
		while(RemainingDistSq > KINDA_SMALL_NUMBER && IdxPathPoint < NumPathPoints)
		{
			const FVector GoalLocation = PathPoints[IdxPathPoint];
			const float SqrDistToNextPoint = FVector::DistSquared(AgentPosition, GoalLocation);
			const FVector MoveStep = (GoalLocation - AgentPosition) * (SqrDistToNextPoint / FMath::Min(RemainingDistSq, SqrDistToNextPoint));

			AgentPosition += MoveStep;
			Movement.steps.Add(MoveStep);
			RemainingDistSq -= SqrDistToNextPoint;

			// If we've exhausted all of the segment, then increment the index. Otherwise, we're going to
			// have incorrect paths that jump ahead before we've reached waypoints.
			if(RemainingDistSq <= 0.01f) { ++IdxPathPoint; }
		}

		// Remove the path from our entity if we've reached the end.
		if(IdxPathPoint >= NumPathPoints - 1)
		{
			flecs::entity Entity = Iterator.entity(Idx);
			//Entity.remove<FNavmeshPathResponse>();
		}
	});
}

void SystemFindNavmeshPath::Iter_NavmeshInput(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const
{
	const UNavigationSystemV1* NavSystem = Cast<UNavigationSystemV1>(UnrealWorld->GetNavigationSystem());
	if(nullptr == NavSystem) { return; }

	ARecastNavMesh* NavData = Cast<ARecastNavMesh>(NavSystem->GetMainNavData());
	if(nullptr == NavData) { return; }

	dtNavMesh* NavMesh = NavData->GetRecastMesh();
	if(nullptr == NavMesh) { return; }
	
	WorkerNavmeshInput[IdxThread].each([this, NavMesh, DeltaTime](const flecs::iter& Iterator,
	                                                              const size_t IdxEntity,
	                                                              const FNavmeshAgent& Agent,
	                                                              const FPosition& Position,
	                                                              const FMovementInput& Input,
	                                                              const FSpeed& Speed,
	                                                              FOneFrameMovementSequence& Movement)
	{
		Movement.steps.Reset();
		
		const FDetourNavigation DetourNavigation(NavMesh);

		double HitTime;
		FVector HitNormal;
		const FVector Start = Position.Value;
		const FVector End = Position.Value + Input.Input * Speed.max * DeltaTime;
			
		const bool bDidHit = DetourNavigation.Raycast(Start, End, Agent.Extents, HitNormal, HitTime);

		const FVector FinalLocation = bDidHit ? End * HitTime : End;

		const_cast<FPosition&>(Position).Value = FinalLocation;
		const_cast<FMovementInput&>(Input).Input = FVector::ZeroVector;
			
		Movement.steps.Add(FinalLocation);
	});
}

int32 SystemFindNavmeshPath::GetCount_PathRequests() const
{
	return QueryPathRequests->count();
}

int32 SystemFindNavmeshPath::GetCount_EnqueueMovement() const
{
	return QueryEnqueueMovement->count();
}

int32 SystemFindNavmeshPath::GetCount_NavmeshInput() const
{
	return QueryNavmeshInput->count();
}

FSystemReadWriteUsage SystemFindNavmeshPath::GetSystemReadWriteUsage_FindPath()
{
	return FSystemReadWriteUsage {
		.Reads = {
			FNavmeshAgent::GetTypeId(),
			FNavmeshPathRequest::GetTypeId(),
		},
		.Writes = {
		}
	};
}

FSystemReadWriteUsage SystemFindNavmeshPath::GetSystemReadWriteUsage_EnqueueMovement()
{
	return FSystemReadWriteUsage {
		.Reads = {
			FNavmeshAgent::GetTypeId(),
			FNavmeshPathRequest::GetTypeId(),
			FPosition::GetTypeId(),
			FMovementInput::GetTypeId(),
			FSpeed::GetTypeId()
		},
		.Writes = {
			FOneFrameMovementSequence::GetTypeId(),
			FNavmeshPathResponse::GetTypeId(),
			FPosition::GetTypeId(),
			FSpeed::GetTypeId(),
			FVelocity::GetTypeId()
		}
	};
}

FSystemReadWriteUsage SystemFindNavmeshPath::GetSystemReadWriteUsage_NavmeshInput()
{
	return FSystemReadWriteUsage {
		.Reads = {
			FNavmeshAgent::GetTypeId(),
			FPosition::GetTypeId(),
			FMovementInput::GetTypeId(),
			FSpeed::GetTypeId()
		},
		.Writes = {
			FOneFrameMovementSequence::GetTypeId(),
		}
	};
}
