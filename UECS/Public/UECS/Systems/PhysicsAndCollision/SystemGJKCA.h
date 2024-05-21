#pragma once

#include "UECS/SystemReadWriteUsage.h"
#include "UECS/Components/PhysicsAndCollision/CollisionSpatialGrid.h"
#include "UECS/Components/PhysicsAndCollision/NarrowPhaseEntityContacts.h"

struct FNarrowPhaseEntityContacts;
struct FNarrowPhaseCollisionCandidates;
struct FCollisionSpatialGrid;
struct FAngularVelocity;
struct FVelocity;
struct FCollisionResults;
struct FTransformComponent;
struct FCollisionGridMember;
struct FPosition;

namespace flecs
{
	struct entity;
	struct world;
	template<typename ... Components> struct query;
	template<typename ... Components> struct worker_iterable;
}

struct FLECSLIBRARY_API FSystemGJKCA
{
	FSystemGJKCA(flecs::world& InEcsWorld);

	void DrawBounds(UWorld* World);
	void Iter_Hashing(const float DeltaTime, flecs::world& FlecsWorld, UWorld* World);
	void Iter_Broadphase(const float DeltaTime, flecs::world& FlecsWorld, int32 IdxThread);
	void Iter_NarrowPhase(float DeltaTime, flecs::world& FlecsWorld, int32 IdxThread);

	void Prep(int32 NumThreads);

	static FSystemReadWriteUsage GetSystemReadWriteUsage();

	TAtomic<int> Iterated { 0 };

private:
	FCollisionSpatialGrid SpatialGrid;
	
	flecs::query<const FCollisionShape, const FPosition, const FVelocity, FNarrowPhaseCollisionCandidates>* QueryBroadPhase { nullptr };
	flecs::query<const FOneFrameMovementSequence, const FCollisionShape, const FVelocity, const FTransformComponent, const FAngularVelocity, FPosition, FNarrowPhaseCollisionCandidates, FNarrowPhaseEntityContacts>* QueryMovePath { nullptr };
	flecs::query<const FCollisionShape, const FTransformComponent, const FVelocity, const FAngularVelocity, FNarrowPhaseCollisionCandidates, FPosition, FNarrowPhaseEntityContacts>* QueryNarrowPhase { nullptr };
	flecs::query<const FPosition, FCollisionGridMember>* QueryChangedMembers { nullptr };
	flecs::query<const FPosition, FNarrowPhaseEntityContacts>* QueryNarrowPhaseCollisionPairs { nullptr };

	TArray<flecs::worker_iterable<const FCollisionShape, const FPosition, const FVelocity, FNarrowPhaseCollisionCandidates>> WorkerBroadPhase;
	TArray<flecs::worker_iterable<const FOneFrameMovementSequence, const FCollisionShape, const FVelocity, const FTransformComponent, const FAngularVelocity, FPosition, FNarrowPhaseCollisionCandidates, FNarrowPhaseEntityContacts>> WorkerMovePath;
	TArray<flecs::worker_iterable<const FCollisionShape, const FTransformComponent, const FVelocity, const FAngularVelocity, FNarrowPhaseCollisionCandidates, FPosition, FNarrowPhaseEntityContacts>> WorkerNarrowPhase;
	TArray<flecs::worker_iterable<const FPosition, FCollisionGridMember>> WorkerChangedMembers;
	TArray<flecs::worker_iterable<const FPosition, FNarrowPhaseEntityContacts>> WorkerNarrowPhaseCollisionPairs;
};
