#pragma once

namespace flecs
{
	struct world;
}

struct UnrealEcsSystem
{
	UWorldSubsystem* ownerSubsys;
	flecs::world* ecsWorld;
	FString name;

	virtual ~UnrealEcsSystem() {} // Needed to allow inherited destructors to free resources safely. Read: virtual destructors.
	
	virtual void initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld)
	{
		ownerSubsys = inOwner;
		ecsWorld = inEcsWorld;
	}
	
	virtual void schedule(class UnrealEcsSystemScheduler* inScheduler) {}
};