#include "UECS/Systems/SystemIntegrateVelocity.h"

#include "UECS/flecs.h"
#include "UECS/Components/BaseComponents.h"

void FSystemIntegrateVelocity::Initialize(UWorldSubsystem* InOwner, flecs::world* InEcsWorld)
{
	UnrealEcsSystem::Initialize(InOwner, InEcsWorld);

	Query = new flecs::query
	(
		InEcsWorld->query_builder<const FVelocity, FPosition>()
			.term<FVelocity>().in()
			.term<FPosition>().inout()
			.build()
	);
}

void FSystemIntegrateVelocity::Iter(const float DeltaTime, flecs::world& World) const
{
	Query->iter(World, [DeltaTime](const flecs::iter& Iterator, const FVelocity* Velocity, FPosition* Position)
	{
		for(const auto IdxEntity : Iterator)
		{
			Position[IdxEntity].Value += Velocity[IdxEntity].Value * DeltaTime;
		}
	});
}

void FSystemIntegrateVelocity::Schedule(UnrealEcsSystemScheduler* InScheduler)
{
	UnrealEcsSystem::Schedule(InScheduler);
}
