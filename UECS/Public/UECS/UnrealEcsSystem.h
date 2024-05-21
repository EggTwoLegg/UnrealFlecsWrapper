#pragma once

namespace flecs
{
	struct world;
}

struct UnrealEcsSystem
{
	UWorldSubsystem* OwnerSubsys { nullptr };
	FString Name;
	flecs::world* EcsWorld { nullptr };

	virtual ~UnrealEcsSystem() {} // Needed to allow inherited destructors to free resources safely. Read: virtual destructors.
	
	virtual void Initialize(UWorldSubsystem* InOwner, flecs::world* InEcsWorld)
	{
		OwnerSubsys = InOwner;
		EcsWorld = InEcsWorld;
	}
	
	virtual void Schedule(class UnrealEcsSystemScheduler* InScheduler) {}

	FORCEINLINE float GetWorldDeltaSeconds() const
	{
		return OwnerSubsys->GetWorld()->GetDeltaSeconds();
	}
};