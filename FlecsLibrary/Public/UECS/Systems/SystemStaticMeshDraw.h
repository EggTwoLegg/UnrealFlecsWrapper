#pragma once

#include "UECS/flecs.h"

#include "UECS/UnrealEcsSystem.h"

struct FLECSLIBRARY_API SystemStaticMeshDraw : public UnrealEcsSystem
{
	virtual void initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld) override;
	virtual void schedule(class UnrealEcsSystemScheduler* inScheduler) override;
	
	flecs::query<struct FInstancedStaticMesh> queryISMOnly;
	flecs::query<const struct FTransformComponent, FInstancedStaticMesh> queryISMAndTransforms;
	flecs::query<const struct FPosition, const struct FRotationComponent, const struct FScale, struct FTransformComponent> queryISMFull;
};
