#include "UECS/EcsWorldSubsystem.h"

#include "UECS/SystemTasks.h"
#include "UECS/UnrealEcsSystem.h"
#include "UECS/Systems/SystemEndBarrier.h"

void UEcsWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEcsWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();

	delete ecsWorld;
	ecsWorld = nullptr;
}

void UEcsWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	scheduler.Get()->Reset();
	scheduler.Get()->ecsWorld = ecsWorld;

	for(auto system : systems)
	{
		system.Get()->schedule(scheduler.Get());
	}
	
	scheduler->Run(*ecsWorld);
}

void UEcsWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	ecsWorld = new flecs::world;
	scheduler = MakeShareable(new UnrealEcsSystemScheduler(ecsWorld, ShouldCacheSystemTaskSetup()));

	RegisterSystem(new SystemEndBarrier());
}

bool UEcsWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UEcsWorldSubsystem::IsTickableInEditor() const
{
	return false;
}

bool UEcsWorldSubsystem::ShouldCacheSystemTaskSetup() const
{
	return false;
}

void UEcsWorldSubsystem::RegisterSystem(UnrealEcsSystem* system)
{
	systems.Add(MakeShareable(system));
	system->initialize(this, ecsWorld);
}

void UEcsWorldSubsystem::AddPrefab(const FName& prefabName, flecs::entity prefabEntity)
{
	ecsPrefabs.Add(prefabName, prefabEntity);
}

bool UEcsWorldSubsystem::GetPrefab(const FName& prefabName, flecs::entity& outPrefab)
{
	auto* find = ecsPrefabs.Find(prefabName);
	if(nullptr == find) { return false; }

	outPrefab = *find;
	return true;
}

bool UEcsWorldSubsystem::SpawnEntityFromPrefab(const FName& prefabName, flecs::entity& outEntity)
{
	flecs::entity prefabEntity;
	const bool bHasPrefab = GetPrefab(prefabName, prefabEntity);
	if(!bHasPrefab) { return false; }

	const flecs::entity instance = ecsWorld->entity().is_a(prefabEntity);
	outEntity = instance;
	return true;
}

TStatId UEcsWorldSubsystem::GetStatId() const
{
	return TStatId();
}
