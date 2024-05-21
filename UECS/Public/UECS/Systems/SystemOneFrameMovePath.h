#pragma once

namespace flecs
{
	struct world;
	template<typename ... Components> struct query;
}

struct FLECSLIBRARY_API SystemOneFrameMovePath
{
	SystemOneFrameMovePath(flecs::world& World);
	void Iter(const float DeltaTime, flecs::world& World) const;

private:
	flecs::query<struct FOneFrameMovementSequence, struct FPosition, struct FRotationComponent>* QueryMoveSequence { nullptr };
};
