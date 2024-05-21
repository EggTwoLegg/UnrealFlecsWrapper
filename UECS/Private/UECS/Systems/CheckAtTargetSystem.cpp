#include "UECS/Systems/CheckAtTargetSystem.h"

#include "UECS/EcsComponentType.h"
#include "UECS/flecs.h"
#include "UECS/SystemReadWriteUsage.h"
#include "UECS/Components/BaseComponents.h"
#include "UECS/Components/MaxTargetDistance.h"
#include "UECS/Components/TargetEntity.h"
#include "UECS/Components/Flags/AtTarget.h"

FCheckAtTargetSystem::FCheckAtTargetSystem(flecs::world& World)
{
	QueryCheckAtTarget = new flecs::query(World.query_builder<const FPosition, const FTargetEntity, const FMaxTargetDistance>()
		.term<FPosition>().in().self()
		.term<FTargetEntity>().in().self()
		.term<FMaxTargetDistance>().in().self()
		.build()
	);
}

void FCheckAtTargetSystem::Iter(const float DeltaTime, flecs::world& World) const
{
	QueryCheckAtTarget->iter(World, [DeltaTime](const flecs::iter& Iterator, const FPosition* Position, const FTargetEntity* TargetEntity, const FMaxTargetDistance* MaxTargetDistance)
	{
		for(const auto IdxEntity : Iterator)
		{
			flecs::entity Entity = Iterator.entity(IdxEntity);

			const float MyRadius = Entity.has<FRadius>() ? Entity.get<FRadius>()->Value : 50.0f;
			const float TargetRadius = TargetEntity[IdxEntity].Value.has<FRadius>() ? TargetEntity[IdxEntity].Value.get<FRadius>()->Value : 50.0f;
			const float MaxTargetDist = MaxTargetDistance[IdxEntity].Value;

			const float TargetDistanceThresholdSquared = (MyRadius + TargetRadius + MaxTargetDist) * (MyRadius + TargetRadius + MaxTargetDist);
			
			const FVector EntityPosition = Position[IdxEntity].Value;
			const FVector TargetPosition = TargetEntity[IdxEntity].Value.get<FPosition>()->Value;
			const float DistanceSquared = FVector::DistSquared(EntityPosition, TargetPosition);

			if(DistanceSquared > TargetDistanceThresholdSquared)
			{
				Entity.remove<FAtTarget>();
				continue;
			}

			Entity.add<FAtTarget>();
		}
	});
}

FSystemReadWriteUsage FCheckAtTargetSystem::GetSystemReadWriteUsage()
{
	return FSystemReadWriteUsage
	{
		.Reads = {
			Position,
			TargetEntity,
			MaxTargetDistance
		},
		
		.Writes ={
		},
	};
}
