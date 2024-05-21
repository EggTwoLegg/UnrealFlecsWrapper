#include "UECS/Systems/SystemOneFrameMovePath.h"

#include "UECS/Components/BaseComponents.h"

SystemOneFrameMovePath::SystemOneFrameMovePath(flecs::world& World)
{
	QueryMoveSequence = new flecs::query(World.query<FOneFrameMovementSequence, FPosition, FRotationComponent>());
}

void SystemOneFrameMovePath::Iter(const float DeltaTime, flecs::world& World) const
{
	QueryMoveSequence->iter(World, [&](const flecs::iter& Iterator, FOneFrameMovementSequence* MoveSequence, FPosition* Position, FRotationComponent*)
	{
		for(const auto IdxEntity : Iterator)
		{
			//auto Entity = Iterator.entity(IdxEntity);
			
			for(const auto Step : MoveSequence[IdxEntity].steps)
			{
				// const FVector StartPosition = Position[IdxEntity].Value;
				Position[IdxEntity].Value = Step;
			}
		}
	});
}
