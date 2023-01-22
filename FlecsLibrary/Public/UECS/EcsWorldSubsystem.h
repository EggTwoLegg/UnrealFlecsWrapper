#pragma once

#include "flecs.h"
#include "UECS/Components/BaseComponents.h"
#include "HierarchicalHashGrid2D.h"
#include "EcsWorldSubsystem.generated.h"

UCLASS()
class FLECSLIBRARY_API UEcsWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

protected:
	TMap<FName, flecs::entity> ecsPrefabs;
	TArray<TSharedPtr<struct UnrealEcsSystem>> systems;
	TSharedPtr<class UnrealEcsSystemScheduler> scheduler;
	flecs::world* ecsWorld;

	TMap<AActor*, flecs::entity> actorToEntityMap;

public:
	THierarchicalHashGrid2D<3, 4, flecs::entity_t> entityHashGrid { 500.0f };

public:
	UEcsWorldSubsystem() {}
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickableInEditor() const override;

	virtual bool ShouldCacheSystemTaskSetup() const;

	void RegisterSystem(struct UnrealEcsSystem* system);
	
	void AddPrefab(const FName& prefabName, flecs::entity prefabEntity);
	bool GetPrefab(const FName& prefabName, flecs::entity& outPrefab);
	bool SpawnEntityFromPrefab(const FName& prefabName, flecs::entity& outEntity);

	FORCEINLINE int32 GetNumPrefabs() const { return ecsPrefabs.Num(); }

	virtual TStatId GetStatId() const override;

	FORCEINLINE flecs::entity RegisterEcsActor(AActor* inActor)
	{
		flecs::entity actorEntity = ecsWorld->entity();
		actorEntity.set<FActorReference>({inActor});
		actorEntity.set<FPosition>({inActor->GetActorLocation()});
		actorEntity.set<FRotationComponent>({inActor->GetActorRotation().Quaternion()});
		actorEntity.set<FScale>({inActor->GetActorScale3D()});
		actorToEntityMap.Add(inActor, actorEntity);

		return actorEntity;
	}

	FORCEINLINE void UnregisterEcsActor(AActor* inActor)
	{
		auto findActor = actorToEntityMap.Find(inActor);
		if(nullptr != findActor)
		{
			findActor->destruct();
			actorToEntityMap.Remove(inActor);
		}
	}
};
