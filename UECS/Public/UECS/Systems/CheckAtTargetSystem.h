#pragma once
#include "UECS/flecs.h"

struct FSystemReadWriteUsage;
struct FMaxTargetDistance;
struct FTargetEntity;
struct FPosition;

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FLECSLIBRARY_API FCheckAtTargetSystem
{
	FCheckAtTargetSystem(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& World) const;

	static FSystemReadWriteUsage GetSystemReadWriteUsage();
	
private:
	flecs::query<const FPosition, const FTargetEntity, const FMaxTargetDistance>* QueryCheckAtTarget { nullptr };
};
