#include "UECS/Systems/SystemEndBarrier.h"

#include "UECS/Components/BaseComponents.h"

SystemEndBarrier::SystemEndBarrier(flecs::world& World)
{
	QueryFlagAdded = new flecs::query(World.query_builder()
		.term<FFlag_Added>().in().self()
		.build());

	QueryFlagChanged = new flecs::query(World.query_builder()
		.term<FFlag_Changed>().in().self()
		.build());
	
	QueryFlagRemove = new flecs::query(World.query_builder()
		.term<FFlag_Removed>().in().self()
		.build());
}

void SystemEndBarrier::Iter(const float DeltaTime, flecs::world& FlecsWorld) const
{
	QueryFlagAdded->iter([&](const flecs::iter& Iterator)
	{
		for(const auto IdxEntity : Iterator)
		{
			flecs::entity CurrentEntity = Iterator.entity(IdxEntity);
			CurrentEntity.remove<FFlag_Added>();
		}
	});
	
	QueryFlagRemove->iter([&](const flecs::iter& Iterator)
	{
		for(const auto IdxEntity : Iterator)
		{
			flecs::entity EntityToRemove = Iterator.entity(IdxEntity);
			EntityToRemove.destruct();
		}
	});
	
	QueryFlagChanged->iter([&](const flecs::iter& Iterator)
	{
		for(const auto IdxEntity : Iterator)
		{
			flecs::entity CurrentEntity = Iterator.entity(IdxEntity);
			CurrentEntity.remove<FFlag_Changed>();
		}
	});
}
