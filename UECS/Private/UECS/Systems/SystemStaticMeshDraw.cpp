#include "UECS/Systems/SystemStaticMeshDraw.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UECS/Components/BaseComponents.h"

SystemStaticMeshDraw::SystemStaticMeshDraw(flecs::world& World)
{
	World.observer<const FTransformComponent, FInstancedStaticMeshMember, FInstancedStaticMesh>()
		.term_at(1).in().self()
		.term_at(2).inout().self()
		.term_at(3).inout().up(flecs::IsA)
		.event(flecs::OnAdd)
		.instanced()
	.each([](const flecs::iter& Iterator, size_t IdxEntity, const FTransformComponent& Transform, FInstancedStaticMeshMember& IsmMember, FInstancedStaticMesh& Ism)
	{
		const auto NumEntitiesMatched = Iterator.count();
		if(NumEntitiesMatched <= 0 || !Ism.UnrealComponent->IsValidLowLevelFast()) { return; }
						
		int32 IdxInIsm = Ism.ChildEntities.Num() - 1;

		TArray<FTransform> NewIsmInstances;
		NewIsmInstances.Reserve(NumEntitiesMatched);
						
		for(const auto IdxEntity : Iterator)
		{
			++IdxInIsm;
			IsmMember.IdxInIsm = IdxInIsm;
							
			NewIsmInstances.Emplace(Transform.Value);

			Ism.ChildEntities.Emplace(Iterator.entity(IdxEntity));
		}

		Ism.UnrealComponent->AddInstances(NewIsmInstances, false, true);
		Ism.UnrealComponent->MarkRenderStateDirty();
	});

	World.observer<const FInstancedStaticMeshMember, FTransformComponent, FInstancedStaticMesh>()
		.term_at(1).in().self()
		.term_at(2).in().self()
		.term_at(3).inout().up(flecs::IsA)
		.event(flecs::OnRemove)
		.instanced()
	.each([](const flecs::iter& Iterator, size_t IdxEntity, const FInstancedStaticMeshMember& IsmMember, FTransformComponent& Transform, FInstancedStaticMesh& Ism)
	{
		const auto NumEntitiesMatched = Iterator.count();

		if(UNLIKELY(NumEntitiesMatched <= 0) || !Ism.UnrealComponent->IsValidLowLevelFast()) { return; }
				
		TArray<int32> RemovedInstanceIds;
		RemovedInstanceIds.Reserve(NumEntitiesMatched);

		const int32 NumIsmInstances = Ism.ChildEntities.Num();
		int32 IdxSwap = NumIsmInstances - 1;
				
		for(const auto IdxEntity : Iterator)
		{
			const int32 IdxOldInIsm = IsmMember.IdxInIsm;

			flecs::entity EndEntity = Ism.ChildEntities[IdxSwap];
			flecs::entity CurrentEntity = Iterator.entity(IdxEntity);
			CurrentEntity.remove<FFlag_Remove>();
			CurrentEntity.add<FFlag_Changed>();
			EndEntity.add<FFlag_Removed>();
					
			// Perform a RemoveAtSwap and remove old entity.
			const FTransform EndTransform = EndEntity.get<FTransformComponent>()->Value;
			Transform.Value = EndTransform;
			Ism.ChildEntities.RemoveAtSwap(IdxSwap);
					
			RemovedInstanceIds.Add(IdxOldInIsm);

			--IdxSwap;
		}

		Ism.UnrealComponent->RemoveInstances(RemovedInstanceIds);
	});

	QueryIsmChanged = new flecs::query(World.query_builder<const FInstancedStaticMeshMember, const FTransformComponent, FInstancedStaticMesh>()
		.term<FFlag_Changed>().in().self()
		.term<FTransformComponent>().in().self()
		.term<FInstancedStaticMeshMember>().in().self()
		.term<FInstancedStaticMesh>().inout().up(flecs::IsA)
		.instanced()
		.build());
}

void SystemStaticMeshDraw::Iter(const float DeltaTime, flecs::world& FlecsWorld) const
{
	QueryIsmChanged->iter(FlecsWorld, [&](const flecs::iter& Iterator, const FInstancedStaticMeshMember* IsmMember, const FTransformComponent* Transform, FInstancedStaticMesh* Ism)
	{
		const auto NumEntitiesMatched = Iterator.count();

		if(UNLIKELY(NumEntitiesMatched <= 0) || !Ism->UnrealComponent->IsValidLowLevelFast()) { return; }
				
		TArray<FTransform> UpdatedInstanceTransforms;
		TArray<int32> UpdatedInstanceIds;
		UpdatedInstanceIds.Reserve(NumEntitiesMatched);
		UpdatedInstanceTransforms.Reserve(NumEntitiesMatched);
		
		for(const auto IdxEntity : Iterator)
		{
			flecs::entity CurrentEntity = Iterator.entity(IdxEntity);
			CurrentEntity.remove<FFlag_Change>();
			CurrentEntity.add<FFlag_Changed>();
					
			const int32 IdxOldInIsm = IsmMember[IdxEntity].IdxInIsm;
			UpdatedInstanceIds.Emplace(IdxOldInIsm);
			UpdatedInstanceTransforms.Emplace(Transform[IdxEntity].Value);
		}

		Ism->UnrealComponent->UpdateInstances(UpdatedInstanceIds, UpdatedInstanceTransforms, UpdatedInstanceTransforms, 0, {});
	});
}
