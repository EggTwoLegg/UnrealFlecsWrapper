#pragma once

#include "UECS/flecs.h"

namespace flecs
{
	struct entity;
	struct world;
	template<typename ... Components> struct query;
	
}

struct FPotentialTargetActor;
struct FTransformComponent;
struct FPosition;
struct FAggroSearchComponent;
struct FTargetActor;
struct FFieldOfViewComponent;
struct FActorComponent;

struct FLECSLIBRARY_API FSystemFindTarget
{
	FSystemFindTarget(flecs::world& World, UWorld* InUnrealWorld);
	void CheckPotentialTargets(flecs::world& World) const;
	void AsyncUnrealOverlapForTargets(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& World);

private:
	flecs::query<const FActorComponent, const FTransformComponent, const FAggroSearchComponent, const FFieldOfViewComponent>* QueryPotentialTargets { nullptr };
	flecs::query<const FActorComponent, const FTransformComponent, const FPotentialTargetActor, const FAggroSearchComponent>* QueryLineOfSight { nullptr };
	UWorld* UnrealWorld { nullptr };

	TMap<FTraceHandle, flecs::entity> AggroCheckMap;

	void OnAggroCheckCallback(const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum);
};


