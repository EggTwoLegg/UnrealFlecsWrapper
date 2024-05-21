#pragma once

#include "UECS/UnrealEcsSystem.h"

struct FVelocity;
struct FPosition;

namespace flecs
{
	template<typename ... Components> struct query;
}

struct FSystemIntegrateVelocity : UnrealEcsSystem
{
	virtual void Initialize(UWorldSubsystem* InOwner, flecs::world* InEcsWorld) override;
	virtual void Schedule(UnrealEcsSystemScheduler* InScheduler) override;
	
	void Iter(float DeltaTime, flecs::world& World) const;
private:
	flecs::query<const FVelocity, FPosition>* Query { nullptr };
};
