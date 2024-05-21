#include "UECS/Systems/SystemFindTarget.h"
#include "UECS/Components/BaseComponents.h"

FSystemFindTarget::FSystemFindTarget(flecs::world& World, UWorld* InUnrealWorld)
{
	QueryPotentialTargets = new flecs::query(World.query_builder<const FActorComponent, const FTransformComponent, const FAggroSearchComponent, const FFieldOfViewComponent>()
		.term<FActorComponent>().in()
		.term<FTransformComponent>().in()
		.term<FAggroSearchComponent>().in()
		.term<FFieldOfViewComponent>().in()
		.build());

	QueryLineOfSight = new flecs::query(World.query_builder<const FActorComponent, const FTransformComponent, const FPotentialTargetActor, const FAggroSearchComponent>()
		.term<FActorComponent>().in()
		.term<FTransformComponent>().in()
		.term<FPotentialTargetActor>().in()
		.term<FAggroSearchComponent>().in()
		.build());
	
	UnrealWorld = InUnrealWorld;
}

void FSystemFindTarget::CheckPotentialTargets(flecs::world& World) const
{
	QueryLineOfSight->iter(World, [this](const flecs::iter& Iterator,
	                                                  const FActorComponent* Actor, const FTransformComponent* Transform, const FPotentialTargetActor* PotentialTarget,
	                                                  const FAggroSearchComponent* AggroComponent)
		{
			for(const auto Idx : Iterator)
			{
				for(const AActor* SearchTarget : PotentialTarget[Idx].PotentialTargets)
				{
					if(!IsValid(SearchTarget)) { continue; }

					flecs::entity Entity = Iterator.entity(Idx);

					const FVector Forward   = Transform[Idx].Value.GetUnitAxis(EAxis::X);
					const FVector RayOffset = AggroComponent[Idx].Offset;
					const FVector Start     = Transform[Idx].Value.GetLocation() + RayOffset;
					const FVector TargetLoc = SearchTarget->GetActorLocation();
					const FQuat   RayBasis  = Forward.ToOrientationQuat();

					FCollisionQueryParams QueryParams;
					QueryParams.AddIgnoredActor(Actor->Actor);

					TSharedPtr<std::atomic<int>> Found = MakeShareable(new std::atomic(0));
					const FTraceDelegate TraceDelegate = FTraceDelegate::CreateLambda([&Entity, &Found, SearchTarget, Start, Forward, TargetLoc]
					(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
						{
							if(*Found > 0) { return; }
							
							for(auto& Hit : TraceDatum.OutHits)
							{
								AActor* HitActor = Hit.GetActor();
								if(IsValid(HitActor) && HitActor == SearchTarget)
								{
									++(*Found);
									Entity.set<FTargetActor>(FTargetActor {.Value = HitActor} );
									Entity.remove<FPotentialTargetActor>();
									break;
								}
							}
						});
						
					constexpr float RayDelta = 60.0f;
					static const FVector Circles[] =
					{
						{1, 0, 0},
						{ 1, FMath::Cos(RayDelta * 1) - FMath::Sin(RayDelta * 1), FMath::Sin(RayDelta * 1) + FMath::Cos(RayDelta * 1) },
						{ 1, FMath::Cos(RayDelta * 2) - FMath::Sin(RayDelta * 2), FMath::Sin(RayDelta * 2) + FMath::Cos(RayDelta * 2) },
						{ 1, FMath::Cos(RayDelta * 3) - FMath::Sin(RayDelta * 3), FMath::Sin(RayDelta * 3) + FMath::Cos(RayDelta * 3) },
						{ 1, FMath::Cos(RayDelta * 4) - FMath::Sin(RayDelta * 4), FMath::Sin(RayDelta * 4) + FMath::Cos(RayDelta * 4) },
						{ 1, FMath::Cos(RayDelta * 5) - FMath::Sin(RayDelta * 5), FMath::Sin(RayDelta * 5) + FMath::Cos(RayDelta * 5) },
						{ 1, FMath::Cos(RayDelta * 6) - FMath::Sin(RayDelta * 6), FMath::Sin(RayDelta * 6) + FMath::Cos(RayDelta * 6) },
					};

					const float Radius = AggroComponent[Idx].Radius;
					for(const auto& Vec : Circles)
					{
						constexpr float RaySpread = 80.0f;
						const FVector Offset = RayBasis * (FVector(Vec.X * Radius, Vec.Y * RaySpread, Vec.Z * RaySpread) + RayOffset);

						const FVector  End = TargetLoc + Offset;
						const FVector  RelativeDirection = (End - Start).GetSafeNormal() * AggroComponent[Idx].Radius;
							
						UnrealWorld->AsyncLineTraceByChannel(EAsyncTraceType::Single, Start, Start + RelativeDirection,
						                                     ECC_Visibility, QueryParams, FCollisionResponseParams::DefaultResponseParam, &TraceDelegate);
					}
				}
			}
		});
}

void FSystemFindTarget::AsyncUnrealOverlapForTargets(flecs::world& World)
{

	QueryPotentialTargets->iter(World, [this] (
		const flecs::iter& Iterator,
		const FActorComponent* Actor,
		const FTransformComponent* TransformComponent,
		const FAggroSearchComponent* AggroSearch,
		const FFieldOfViewComponent* FOVComponent)
		{
			AggroCheckMap.Reset();
			
			for(const auto IdxEntity : Iterator)
			{
				const FCollisionShape AggroOverlapShape = FCollisionShape::MakeSphere(AggroSearch[IdxEntity].Radius);
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(Actor->Actor);

				const FAggroSearchComponent& EntityAggroComponent = AggroSearch[IdxEntity];
				const FTransform Transform     = TransformComponent[IdxEntity].Value;
				const FVector    StartLocation = Transform.GetLocation();
				
				const FOverlapDelegate OverlapDelegate = FOverlapDelegate::CreateRaw(this, &FSystemFindTarget::OnAggroCheckCallback);
				const FTraceHandle OverlapTraceHandle = UnrealWorld->AsyncOverlapByChannel(StartLocation,
					FQuat::Identity,
					EntityAggroComponent.CollisionChannel,
					AggroOverlapShape,
					QueryParams,
					FCollisionResponseParams::DefaultResponseParam,
					&OverlapDelegate);

				AggroCheckMap.Emplace(OverlapTraceHandle, Iterator.entity(IdxEntity));
			}
		});
}

void FSystemFindTarget::Iter(const float DeltaTime, flecs::world& World)
{
	AsyncUnrealOverlapForTargets(World);
	CheckPotentialTargets(World);
}

void FSystemFindTarget::OnAggroCheckCallback(const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum)
{
	flecs::entity* Entity = AggroCheckMap.Find(TraceHandle);
	
	if(!Entity->is_alive() || OverlapDatum.OutOverlaps.Num() < 1) { return; }

	const FVector StartLocation = OverlapDatum.Pos;
	const FVector Forward = Entity->get<FTransformComponent>()->Value.GetUnitAxis(EAxis::X);
	const float FOV = Entity->get<FFieldOfViewComponent>()->Value;
							

	TArray<FName> RequiredTags {};
	if(Entity->has<FAggroSearchComponent>())
	{
		const FAggroSearchComponent* EntityAggroComponent = Entity->get<FAggroSearchComponent>();
		RequiredTags = EntityAggroComponent->RequiredTags;
	}
	
	TArray<AActor*> OverlappedActors;
	OverlappedActors.Reserve(OverlapDatum.OutOverlaps.Num());
	
	for(auto& Overlap : OverlapDatum.OutOverlaps)
	{
		const AActor* OverlappedActor = Overlap.GetActor();
		if(IsValid(OverlappedActor))
		{
			const FVector TargetLocation	= OverlappedActor->GetActorLocation();
			const FVector RelativeDirection = TargetLocation - StartLocation;
															
			const float Sin = FVector::DotProduct({0.0f, 0.0f, 1.0f}, FVector::CrossProduct(Forward, RelativeDirection));
			const float Cos = FVector::DotProduct(Forward, RelativeDirection);
			const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
			if(FMath::Abs(SignedAngle) > FOV) { continue; }
			
			for(auto& Tag : RequiredTags)
			{
				if(!OverlappedActor->Tags.Contains(Tag))
				{
					OverlappedActors.Add(Overlap.GetActor());
					break;
				}
			}
		}
	}

	const FEntityCustomTargetSorting* CustomTargetSorting = Entity->get<FEntityCustomTargetSorting>();
	if(nullptr != CustomTargetSorting)
	{
		CustomTargetSorting->Sorter(OverlappedActors);
	}

	AActor* TargetActor = OverlappedActors[0];
						
	if(!IsValid(TargetActor)) { return; }
	
	Entity->set<FPotentialTargetActor>( { .PotentialTargets = { TargetActor } });
}
