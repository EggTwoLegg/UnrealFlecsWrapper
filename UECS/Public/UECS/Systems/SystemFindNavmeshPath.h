#pragma once

#include "UECS/SystemReadWriteUsage.h"

struct FMovementInput;

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
	template<typename ... Components> struct worker_iterable;
}

struct FLECSLIBRARY_API SystemFindNavmeshPath
{
	SystemFindNavmeshPath(flecs::world& World);
	void Prep(int32 NumThreads);
	void Iter_FindPath(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const;
	void Iter_EnqueueMovement(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const;
	void Iter_NavmeshInput(const float DeltaTime, flecs::world& World, UWorld* UnrealWorld, const int32 IdxThread) const;

	int32 GetCount_PathRequests() const;
	int32 GetCount_EnqueueMovement() const;
	int32 GetCount_NavmeshInput() const;

	static FSystemReadWriteUsage GetSystemReadWriteUsage_FindPath();
	static FSystemReadWriteUsage GetSystemReadWriteUsage_EnqueueMovement();
	static FSystemReadWriteUsage GetSystemReadWriteUsage_NavmeshInput();

	
private:
	flecs::query<const struct FNavmeshAgent, const struct FNavmeshPathRequest>* QueryPathRequests { nullptr };
	flecs::query<struct FNavmeshPathResponse, struct FOneFrameMovementSequence, struct FPosition, struct FSpeed, struct FVelocity>* QueryEnqueueMovement { nullptr };
	flecs::query<const FNavmeshAgent, const FPosition, const FMovementInput, const FSpeed, FOneFrameMovementSequence>* QueryNavmeshInput { nullptr };

	TArray<flecs::worker_iterable<const struct FNavmeshAgent, const struct FNavmeshPathRequest>> WorkerPathRequests {};
	TArray<flecs::worker_iterable<struct FNavmeshPathResponse, struct FOneFrameMovementSequence, struct FPosition, struct FSpeed, struct FVelocity>> WorkerEnqueueMovement {};
	TArray<flecs::worker_iterable<const FNavmeshAgent, const FPosition, const FMovementInput, const FSpeed, FOneFrameMovementSequence>> WorkerNavmeshInput {};
};
