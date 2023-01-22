#pragma once

#include "UECS/UnrealEcsSystem.h"
#include "UECS/flecs.h"

struct SystemPackTransforms : public UnrealEcsSystem
{
	virtual void initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld) override;
	virtual void schedule(UnrealEcsSystemScheduler* inScheduler) override;

private:
	flecs::query<const struct FPosition, const struct FRotationComponent, const struct FScale, struct FTransformComponent> queryTransforms; 
};
