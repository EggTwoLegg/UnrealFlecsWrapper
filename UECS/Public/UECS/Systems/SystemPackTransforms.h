#pragma once

struct FActorComponent;

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FLECSLIBRARY_API SystemPackTransforms
{
	SystemPackTransforms(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& FlecsWorld) const;

private:
	flecs::query<const struct FPosition, const struct FRotationComponent, const struct FScale, const struct FTransformComponent, struct FTransformComponent>* QueryTransforms { nullptr };
	flecs::query<FActorComponent, const FTransformComponent>* QueryEcsToUnrealActors { nullptr };
	flecs::query<const FActorComponent, FPosition, FRotationComponent, FScale>* QueryActorsToEcs { nullptr };
};
