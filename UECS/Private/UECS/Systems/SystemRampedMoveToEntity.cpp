#include "UECS/Systems/SystemRampedMoveToEntity.h"

#include "FlecsLibrary.h"

#include "UECS/EcsComponentType.h"
#include "UECS/Components/RampedMoveToEntity.h"
#include "UECS/Components/BaseComponents.h"

DECLARE_CYCLE_STAT(TEXT("SystemRampedMoveToEntity"), CS_SYSTEM_MOVE_TO_ENTITY, STATGROUP_ECS)

FSystemRampedMoveToEntity::FSystemRampedMoveToEntity(flecs::world& World)
{
	QueryRampedMove = new flecs::query(
		World.query_builder<const FPosition, FVelocity, FRampedMoveToEntity>()
			.term<FPosition>().in().self()
			.term<FVelocity>().inout().self()
			.term<FRampedMoveToEntity>().inout().self()
			.build()
	);
}

void FSystemRampedMoveToEntity::Iter(const float DeltaTime, flecs::world& World) const
{
	SCOPE_CYCLE_COUNTER(CS_SYSTEM_MOVE_TO_ENTITY);
	
	auto IterationFn = [DeltaTime](const flecs::iter& Iterator, const FPosition* Position, FVelocity* Velocity, FRampedMoveToEntity* RampedMoveToEntity)
	{
		for(const auto IdxEntity : Iterator)
		{
			const flecs::entity TargetEntity = RampedMoveToEntity[IdxEntity].Target;
			if(!TargetEntity.is_alive()) { continue; }

			RampedMoveToEntity[IdxEntity].RampTime = FMath::Min(RampedMoveToEntity[IdxEntity].RampTime + DeltaTime, RampedMoveToEntity[IdxEntity].RampMaxTime);
			const float Speed = (RampedMoveToEntity[IdxEntity].RampTime / RampedMoveToEntity[IdxEntity].RampMaxTime) * RampedMoveToEntity[IdxEntity].MaxSpeed;

			const FVector TargetPosition = TargetEntity.get<FPosition>()->Value;
			const FVector CurrentPosition = Position[IdxEntity].Value;
			const float DistanceSquared = FVector::DistSquared(TargetPosition, CurrentPosition);
			const FVector Direction = (TargetPosition - CurrentPosition).GetSafeNormal();

			// Avoid overshooting the target. We don't factor DeltaTime in here, as it is handled by the velocity component in another system.
			const FVector NewPosition = DistanceSquared > Speed * Speed ? CurrentPosition + (Direction * Speed) : TargetPosition;
			Velocity[IdxEntity].Value = NewPosition - CurrentPosition;
		}
	};
	
	QueryRampedMove->iter(World, IterationFn);
}

FSystemReadWriteUsage FSystemRampedMoveToEntity::GetSystemReadWriteUsage()
{
	return {
		.Reads = {
			EEcsComponentType::Position,
			EEcsComponentType::Velocity,
			EEcsComponentType::RampedMoveToEntity
		},
		.Writes = {
			EEcsComponentType::Velocity,
			EEcsComponentType::RampedMoveToEntity
		} 
	};
}
