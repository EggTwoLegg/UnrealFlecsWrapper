#pragma once


namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FTransformComponent;
struct FInstancedStaticMesh;
struct FInstancedStaticMeshMember;

struct FLECSLIBRARY_API SystemStaticMeshDraw
{
	SystemStaticMeshDraw(flecs::world& World);

	void Iter(const float DeltaTime, flecs::world& FlecsWorld) const;

private:
	flecs::query<const FInstancedStaticMeshMember, const FTransformComponent, FInstancedStaticMesh>* QueryIsmChanged { nullptr };
};
