#pragma once

#include "UECS/SystemReadWriteUsage.h"
#include "UECS/UnrealEcsSystem.h"

struct FVelocity;
struct FRampedMoveToEntity;
struct FPosition;

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FLECSLIBRARY_API FSystemRampedMoveToEntity
{
	FSystemRampedMoveToEntity(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& World) const;

	static FSystemReadWriteUsage GetSystemReadWriteUsage();
	
private:
	flecs::query<const FPosition, FVelocity, FRampedMoveToEntity>* QueryRampedMove { nullptr };
};


