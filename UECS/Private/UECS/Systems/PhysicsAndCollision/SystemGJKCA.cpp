#include "UECS/Systems/PhysicsAndCollision/SystemGJKCA.h"

#include "FlecsLibrary.h"
#include "UECS/EcsComponentType.h"
#include "UECS/CollisionHelper.h"
#include "UECS/Components/AngularVelocity.h"
#include "UECS/Components/BaseComponents.h"
#include "UECS/Components/Stationary.h"
#include "UECS/Components/PhysicsAndCollision/CollisionEnabled.h"
#include "UECS/Components/PhysicsAndCollision/CollisionSpatialGrid.h"
#include "UECS/Components/PhysicsAndCollision/CollisionGridMember.h"
#include "UECS/Components/PhysicsAndCollision/ICollisionHandler.h"
#include "UECS/Components/PhysicsAndCollision/NarrowPhaseCollisionCandidates.h"
#include "UECS/Components/PhysicsAndCollision/NarrowPhaseEntityContacts.h"
#include "UECS/Components/PhysicsAndCollision/OverlapCollision.h"

DECLARE_CYCLE_STAT(TEXT("SystemCollisionBroadPhase"), CS_SYSTEM_COLLISION_BROADPHASE, STATGROUP_ECS)
DECLARE_CYCLE_STAT(TEXT("SystemCollisionNarrowPhase"), CS_SYSTEM_COLLISION_NARROWPHASE, STATGROUP_ECS)
DECLARE_CYCLE_STAT(TEXT("SystemCollisionHashing"), CS_SYSTEM_COLLISION_HASHING, STATGROUP_ECS)

FSystemGJKCA::FSystemGJKCA(flecs::world& World)
{
	World.observer<const FPosition>()
		.term<FPosition>().in().self()
		.term<FCollisionShape>().in().self()
		.term<FCollisionEnabled>().in().self()
		.yield_existing(true)
	.event(flecs::OnAdd).each([this](const flecs::iter& Iterator, uint64 IdxEntity, const FPosition& Position)
	{
		flecs::entity Entity = Iterator.entity(IdxEntity);
		SpatialGrid.Add<FCollisionGridMember>(Entity, Position);
		Entity.add<FNarrowPhaseCollisionCandidates>();
		Entity.add<FNarrowPhaseEntityContacts>();
		// Print position
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("Position: %s"), *Position.Value.ToString()));
	});

	World.observer<const FCollisionGridMember>()
		.term<FCollisionGridMember>().in().self()
	.event(flecs::OnRemove).each([](const flecs::iter& Iterator, uint64 IdxEntity, const FCollisionGridMember& SpatialHashMember)
	{
		flecs::entity Entity = Iterator.entity(IdxEntity);
		
		FCollisionSpatialGrid* CollisionSpatialGrid = Iterator.world().get_mut<FCollisionSpatialGrid>();
		if(nullptr != CollisionSpatialGrid)
		{
			CollisionSpatialGrid->Remove(SpatialHashMember);
			return;
		}

		if(Entity.is_alive())
		{
			Entity.remove<FNarrowPhaseCollisionCandidates>();
			Entity.remove<FNarrowPhaseEntityContacts>();
		}
	});

	QueryChangedMembers = new flecs::query(
		World.query_builder<const FPosition, FCollisionGridMember>()
		          .term<FPosition>().in().self()
		          .term<FCollisionGridMember>().in().self()
		          .term<FStationary>().not_()
		          .build()
	);
	
	QueryBroadPhase = new flecs::query(
		World.query_builder<const FCollisionShape, const FPosition, const FVelocity, FNarrowPhaseCollisionCandidates>()
		          .term_at(3).optional().self()
		          .term<FCollisionShape>().in().self()
		          .term<FPosition>().in().self()
		          .term<FCollisionEnabled>().in().self()
		          .term<FStationary>().not_()
		          .build()
	);

	QueryNarrowPhase = new flecs::query(
		World.query_builder<const FCollisionShape, const FTransformComponent, const FVelocity, const FAngularVelocity, FNarrowPhaseCollisionCandidates, FPosition, FNarrowPhaseEntityContacts>()
		          .term<FCollisionShape>().in().self()
		          .term<FTransformComponent>().in().self()
		          .term<FVelocity>().in().self()
		          .term<FAngularVelocity>().in().self()
		          .term<FNarrowPhaseCollisionCandidates>().in().self()
		          .term<FPosition>().inout().self()
		          .term<FNarrowPhaseEntityContacts>().out().self()
		          .term<FOneFrameMovementSequence>().not_()
		          .term<FOverlapCollision>().in().self().optional()
		          .term<FCollisionHandler>().in().self().optional()
		          .term<FStationary>().not_()
		          .build()
	);

	QueryMovePath = new flecs::query(
		World.query_builder<const FOneFrameMovementSequence, const FCollisionShape, const FVelocity, const FTransformComponent, const FAngularVelocity, FPosition, FNarrowPhaseCollisionCandidates, FNarrowPhaseEntityContacts>()
				  .term<FOneFrameMovementSequence>().inout().self()
				  .term<FCollisionShape>().in().self()
		          .term<FPosition>().in().self()
		          .term<FVelocity>().in().self()
		          .term<FTransformComponent>().in().self()
		          .term<FVelocity>().in().self()
		          .term<FAngularVelocity>().in().self()
		          .term<FPosition>().inout().self()
		          .term<FNarrowPhaseCollisionCandidates>().out().self()
		          .term<FNarrowPhaseEntityContacts>().out().self()
		          .term<FStationary>().not_()
		          .build()
	);

	QueryNarrowPhaseCollisionPairs = new flecs::query(
		World.query_builder<const FPosition, FNarrowPhaseEntityContacts>()
		          .term<FPosition>().in().self()
		          .term<FNarrowPhaseEntityContacts>().in().self()
		          .term<FStationary>().not_()
		          .build()
	);

	SpatialGrid.PartitionSize = 1600.0f;
}

void FSystemGJKCA::Prep(int32 NumThreads)
{
	WorkerChangedMembers.Reset(NumThreads);
	WorkerBroadPhase.Reset(NumThreads);
	WorkerNarrowPhase.Reset(NumThreads);
	WorkerMovePath.Reset(NumThreads);
	WorkerNarrowPhaseCollisionPairs.Reset(NumThreads);
	for(int32 IdxThread = 0; IdxThread < NumThreads; ++IdxThread)
	{
		WorkerChangedMembers.Add(QueryChangedMembers->worker(IdxThread, NumThreads));
		WorkerBroadPhase.Add(QueryBroadPhase->worker(IdxThread, NumThreads));
		WorkerNarrowPhase.Add(QueryNarrowPhase->worker(IdxThread, NumThreads));
		WorkerMovePath.Add(QueryMovePath->worker(IdxThread, NumThreads));
		WorkerNarrowPhaseCollisionPairs.Add(QueryNarrowPhaseCollisionPairs->worker(IdxThread, NumThreads));
	}
}

void HemisphereBroadphase(float DeltaTime, const flecs::iter& Iterator, const FCollisionShape* CollisionShape, const FTransformComponent* Transform, const FVelocity* Velocity, FCollisionSpatialGrid& CollisionSpatialGrid, FNarrowPhaseCollisionCandidates* NarrowPhaseCollisionCandidates)
{
	thread_local TArray<FEntityPositionCache> EntitiesInHemisphere;
	
	for (const auto IdxEntity : Iterator)
	{
		const FCollisionShape& Shape = CollisionShape[IdxEntity];
		const FTransformComponent& TargetTransform = Transform[IdxEntity];
		const FVector MyPosition = TargetTransform.Value.GetLocation();
		const FVelocity& TargetVelocity = Velocity[IdxEntity];
		const float BoundingRadius = FCollisionHelper::GetBoundingRadius(Shape, TargetTransform.Value);
		const float Radius = (TargetVelocity.Value * DeltaTime).Length() + BoundingRadius;

		flecs::entity Entity = Iterator.entity(IdxEntity);
		
		EntitiesInHemisphere.SetNum(0, false);
		CollisionSpatialGrid.GetEntitiesInHemisphere(Entity, MyPosition, Radius, TargetVelocity.Value.GetSafeNormal(), EntitiesInHemisphere);
		
		EntitiesInHemisphere.Sort([MyPosition](const FEntityPositionCache& A, const FEntityPositionCache& B)
		{
			const float DistanceA = (A.Position - MyPosition).SizeSquared();
			const float DistanceB = (B.Position - MyPosition).SizeSquared();
			
			return DistanceA < DistanceB;
		});

		NarrowPhaseCollisionCandidates[IdxEntity].Entities = EntitiesInHemisphere;
	}
}

void FSystemGJKCA::DrawBounds(UWorld* World)
{
	SpatialGrid.DrawBounds(World);
	
	QueryBroadPhase->each([World](const flecs::iter& Iterator,
		const size_t i,
	                              const FCollisionShape& CollisionShape,
	                              const FPosition& Position,
	                              const FVelocity& Velocity,
	                              FNarrowPhaseCollisionCandidates& NarrowPhaseCollisionCandidates)
	{
		switch(CollisionShape.ShapeType)
		{
		case ECollisionShape::Box:
			DrawDebugBox(World, Position.Value, CollisionShape.GetExtent(), FColor::Red, false, -1);
			break;
		case ECollisionShape::Sphere:
			DrawDebugSphere(World, Position.Value, CollisionShape.GetSphereRadius(), 16, FColor::Red, false, -1);
			break;
		case ECollisionShape::Capsule:
			DrawDebugCapsule(World, Position.Value, CollisionShape.GetExtent().Z, CollisionShape.GetExtent().X, FQuat::Identity, FColor::Red, false, -1);
			break;
		default: ;
		}
	});
}

void FSystemGJKCA::Iter_Hashing(const float DeltaTime, flecs::world& FlecsWorld, UWorld* World)
{
	{
		SCOPE_CYCLE_COUNTER(CS_SYSTEM_COLLISION_HASHING)
		
		QueryChangedMembers->each(FlecsWorld, [this](const flecs::entity& Entity,
			const FPosition& Position,
			FCollisionGridMember& SpatialHashMember)
		{
			SpatialGrid.Change<FCollisionGridMember>(const_cast<flecs::entity&>(Entity), Position, SpatialHashMember);
		});
	}
}

void FSystemGJKCA::Iter_Broadphase(const float DeltaTime, flecs::world& FlecsWorld, int32 IdxThread)
{
	{
		SCOPE_CYCLE_COUNTER(CS_SYSTEM_COLLISION_BROADPHASE)
		
		WorkerBroadPhase[IdxThread].each(FlecsWorld, [DeltaTime, this](const flecs::entity& Entity,
		                                                        const FCollisionShape& CollisionShape,
		                                                        const FPosition& Position,
		                                                        const FVelocity& Velocity,
		                                                        FNarrowPhaseCollisionCandidates& NarrowPhaseCollisionCandidates)
		{
			FCollisionHelper::BoxBroadphase(DeltaTime, Entity, CollisionShape, Position, Velocity, SpatialGrid, NarrowPhaseCollisionCandidates.Entities);
		});
	}
}

void FSystemGJKCA::Iter_NarrowPhase(const float DeltaTime, flecs::world& FlecsWorld, int32 IdxThread)
{
	{
		SCOPE_CYCLE_COUNTER(CS_SYSTEM_COLLISION_NARROWPHASE)
		
		WorkerNarrowPhase[IdxThread].each(FlecsWorld, [DeltaTime, this](const flecs::entity& Entity,						
		                                                     const FCollisionShape& CollisionShape,
		                                                     const FTransformComponent& Transform,
		                                                     const FVelocity& Velocity,
		                                                     const FAngularVelocity& AngularVelocity,
		                                                     FNarrowPhaseCollisionCandidates& NarrowPhaseCollisionCandidates,
		                                                     FPosition& Position,
															 FNarrowPhaseEntityContacts& NarrowPhaseContacts)
		{
			float HitTime = 0.0f;
			FCollisionHelper::NarrowPhase(
				DeltaTime,
				Entity,
				CollisionShape,
				Transform,
				Velocity.Value,
				AngularVelocity,
				NarrowPhaseCollisionCandidates.Entities,
				Position,
				NarrowPhaseContacts.Contacts,
				HitTime);
		
			Position.Value = Position.Value + Velocity.Value * DeltaTime * HitTime;

			NarrowPhaseCollisionCandidates.Entities.Reset();
		});

		WorkerMovePath[IdxThread].each(FlecsWorld, [DeltaTime, this](const flecs::iter& Iterator,
													 const size_t IdxEntity,
													 const FOneFrameMovementSequence& MovementSequence,
													 const FCollisionShape& CollisionShape,
													 const FVelocity& Velocity,
													 const FTransformComponent& Transform,
													 const FAngularVelocity& AngularVelocity,
													 FPosition& Position,
													 FNarrowPhaseCollisionCandidates& NarrowPhaseCollisionCandidates,
													 FNarrowPhaseEntityContacts& NarrowPhaseContacts)
		{
			const flecs::entity Entity = Iterator.entity(IdxEntity);

			// Print num contacts
			for(int32 IdxStep = 0; IdxStep < MovementSequence.steps.Num(); ++IdxStep)
			{
				const FVector Step = MovementSequence.steps[IdxStep];
				float HitTime = 0.0f;
				FCollisionHelper::NarrowPhase(
					DeltaTime,
					Entity,
					CollisionShape,
					Transform,
					Step,
					AngularVelocity,
					NarrowPhaseCollisionCandidates.Entities,
					Position,
					NarrowPhaseContacts.Contacts,
					HitTime
				);
		
				Position.Value += Velocity.Value * DeltaTime * HitTime;
			}

			NarrowPhaseCollisionCandidates.Entities.Reset();
		});

		WorkerNarrowPhaseCollisionPairs[IdxThread].each(FlecsWorld, [this](const flecs::iter& Iterator,
			const size_t IdxEntity,
			const FPosition& Position,
			FNarrowPhaseEntityContacts& NarrowPhaseEntityContacts)
		{
			const flecs::entity Entity = Iterator.entity(IdxEntity);
			for(auto& Candidate : NarrowPhaseEntityContacts.Contacts)
			{
				// GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Iter_NarrowPhase"));
				const flecs::entity& EntityHit = Candidate.EntityHit;
				const bool bHasCollisionHandler = EntityHit.has<FCollisionHandler>();
				if(!bHasCollisionHandler) { continue; }
		
				const FCollisionHandler* CollisionHandler = EntityHit.get<FCollisionHandler>();
				CollisionHandler->Handler->Handle(Candidate, Entity);
			}
		
			NarrowPhaseEntityContacts.Contacts.Reset();
		});
	}
}

FSystemReadWriteUsage FSystemGJKCA::GetSystemReadWriteUsage()
{
	return FSystemReadWriteUsage {
		.Reads = {
			EEcsComponentType::Position,
			EEcsComponentType::CollisionShape,
			EEcsComponentType::TransformComponent,
			EEcsComponentType::Velocity,
			EEcsComponentType::AngularVelocity,
			EEcsComponentType::CollisionEnabled,
			EEcsComponentType::NarrowPhaseCollisionCandidates,
			EEcsComponentType::CollisionGridMember
		},
		.Writes = {
			EEcsComponentType::Position,
			EEcsComponentType::NarrowPhaseEntityContacts
		}
	};
}
