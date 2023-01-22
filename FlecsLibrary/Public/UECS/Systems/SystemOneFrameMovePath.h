#pragma once

#include "UECS/flecs.h"
#include "UECS/UnrealEcsSystem.h"

struct FLECSLIBRARY_API SystemOneFrameMovePath : public UnrealEcsSystem
{
	virtual void initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld) override;
	virtual void schedule(UnrealEcsSystemScheduler* inScheduler) override;

private:
	flecs::query<struct FOneFrameMovementSequence, struct FPosition, struct FRotationComponent> queryMoveSequence;
};
