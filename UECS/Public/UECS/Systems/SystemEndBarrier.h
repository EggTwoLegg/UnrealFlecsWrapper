#pragma once

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FFlag_Removed;
struct FFlag_Changed;
struct FFlag_Added;

struct FLECSLIBRARY_API SystemEndBarrier
{
	SystemEndBarrier(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& FlecsWorld) const;

private:
	flecs::query<>* QueryFlagAdded { nullptr };
	flecs::query<>* QueryFlagChanged { nullptr };
	flecs::query<>* QueryFlagRemove { nullptr };
};
