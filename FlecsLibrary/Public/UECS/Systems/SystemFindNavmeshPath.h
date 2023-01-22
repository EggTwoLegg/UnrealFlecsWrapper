#pragma once

#include "UECS/flecs.h"
#include "UECS/UnrealEcsSystem.h"

struct FLECSLIBRARY_API SystemFindNavmeshPath : public UnrealEcsSystem
{
	virtual void initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld) override;
	virtual void schedule(UnrealEcsSystemScheduler* inScheduler) override;

private:
	flecs::query<struct FNavmeshAgent, struct FNavmeshPathRequest> queryPathRequests;
	flecs::query<struct FNavMeshPath, struct FOneFrameMovementSequence, struct FPosition, struct FSpeed, struct FVelocity> queryAgentPositions;

	TMap<uint32, flecs::entity> entityPathQueryIdMap;

	FNavPathQueryDelegate pathCompletedCallback;
};
