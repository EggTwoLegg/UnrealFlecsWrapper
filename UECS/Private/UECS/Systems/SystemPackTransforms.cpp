#include "UECS/Systems/SystemPackTransforms.h"

#include "UECS/Components/BaseComponents.h"
#include "UECS/Components/SyncTransformBackToUnreal.h"
#include "UECS/Components/SyncTransformsFromUnrealToEcs.h"

SystemPackTransforms::SystemPackTransforms(flecs::world& World)
{
	QueryTransforms = new flecs::query(World.query_builder<const FPosition, const FRotationComponent, const FScale, const FTransformComponent, FTransformComponent>()
		.term_at(1).in().self()
		.term_at(2).in().self()
		.term_at(3).in().self()
		.term_at(4).in().parent().optional().cascade()
		.term_at(5).out().self()
		.build());

	QueryEcsToUnrealActors = new flecs::query(World.query_builder<FActorComponent, const FTransformComponent>()
		.term<FActorComponent>().in()
		.term<FTransformComponent>().in()
		.term<FSyncTransformBackToUnreal>().in()
		.build());

	QueryActorsToEcs = new flecs::query(World.query_builder<const FActorComponent, FPosition, FRotationComponent, FScale>()
		.term<FActorComponent>().in()
		.term<FPosition>().out()
		.term<FRotationComponent>().out()
		.term<FScale>().out()
		.term<FSyncTransformsFromUnrealToEcs>().in()
		.build());
}

void SystemPackTransforms::Iter(const float DeltaTime, flecs::world& FlecsWorld) const
{
	QueryTransforms->iter(FlecsWorld, [](const flecs::iter& Iterator, const FPosition* Position, const FRotationComponent* Rotation, const FScale* Scale, const FTransformComponent* ParentTransform, FTransformComponent* Transform)
	{
		const bool bHasParentTransform = nullptr != ParentTransform;
		
		for(const auto IdxEntity : Iterator)
		{
			FVector EntityPosition = Position[IdxEntity].Value;
			FQuat EntityRotation = Rotation[IdxEntity].Value;
			FVector EntityScale = Scale[IdxEntity].value;
			
			if(bHasParentTransform)
			{
				EntityPosition += ParentTransform->Value.GetTranslation();
				// Print offset position
				GEngine->AddOnScreenDebugMessage(0, 5.0f, FColor::Red, FString::Printf(TEXT("Offset Position: %s"), *EntityPosition.ToString()));

				// Print parent position
				GEngine->AddOnScreenDebugMessage(1, 5.0f, FColor::Red, FString::Printf(TEXT("Parent Position: %s"), *ParentTransform->Value.GetTranslation().ToString()));
			}
			
			Transform[IdxEntity].Value.SetComponents(EntityRotation, EntityPosition, EntityScale);
		}
	});
	
	QueryEcsToUnrealActors->iter(FlecsWorld, [](const flecs::iter& Iterator, FActorComponent* Actor, const FTransformComponent* Transform)
	{
		for(const auto IdxEntity : Iterator)
		{
			Actor[IdxEntity].Actor->SetActorTransform(Transform[IdxEntity].Value);
		}
	});

	QueryActorsToEcs->iter(FlecsWorld,[](const flecs::iter& Iterator, const FActorComponent* Actor, FPosition* Position, FRotationComponent* Rotation, FScale* Scale)
	{
		for(const auto IdxEntity : Iterator)
		{
			const FTransform& Transform = Actor[IdxEntity].Actor->GetActorTransform();
			Position[IdxEntity].Value = Transform.GetLocation();
			Rotation[IdxEntity].Value = Transform.GetRotation();
			Scale[IdxEntity].value = Transform.GetScale3D();
		}
	});
	
}
