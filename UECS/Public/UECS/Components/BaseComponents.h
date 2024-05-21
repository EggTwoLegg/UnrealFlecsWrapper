#pragma once

#include "UECS/EcsComponentType.h"
#include "UECS/IsmEntityOverlap.h"

struct FISMIndexHit;

struct FFlag_Add{};
struct FFlag_Added{};
struct FFlag_Remove{};
struct FFlag_Removed{};
struct FFlag_Change{};
struct FFlag_Changed{};

struct FFlag_WantsTarget {};

struct FFlag_CollisionEnabled {};

struct FCollisionMask
{
	int32 Mask { 0 };
};

struct FActorComponent
{
	AActor* Actor { nullptr };
};

struct FPosition
{
	FVector Value;
	
	FORCEINLINE static int32 GetTypeId()
	{
		return EEcsComponentType::Position;
	}
};

struct FRotationComponent
{
	FQuat Value { FQuat::Identity }; 
};

struct FScale
{
	FVector value;
};

struct FVelocity
{
	FVector Value;

	void Add(const FVector& OtherVelocity) { Value += OtherVelocity; }

	FORCEINLINE static int32 GetTypeId() { return EEcsComponentType::Velocity; }
};

struct FSpeed
{
	float max;
	float linearAcceleration;
	
	FORCEINLINE static int32 GetTypeId() { return EEcsComponentType::Speed; }
};

struct FOneFrameMovementSequence
{
	TArray<FVector, TInlineAllocator<8>> steps;

	FORCEINLINE static int32 GetTypeId()
	{
		return EEcsComponentType::OneFrameMovementSequence;
	}
};

struct FRadius
{
	float Value;
};

struct FHunger
{
	float CurrentHunger { 0.0f };
	float MaxHunger { 0.0f };
	float GainRate { 0.0f };
};

struct FCooldown
{
	float Current { 0.0f };
	float MaxCooldown { 0.0f };
};

struct FSkeletalMesh
{
	USkeletalMeshComponent* Value { nullptr };
};

struct FInstancedStaticMesh
{
	UInstancedStaticMeshComponent* UnrealComponent { nullptr };
	TArray<flecs::entity> ChildEntities;
};

struct FInstancedStaticMeshMember
{
	int32 IdxInIsm { 0 };
};

struct FTransformComponent
{
	FTransform Value; 
};

struct FNavmeshAgent
{
	FVector Extents;
	
	FORCEINLINE static int32 GetTypeId() { return EEcsComponentType::NavmeshAgent; }
};

struct FOverlapComponent
{
	TArray<flecs::entity, TInlineAllocator<4>> EntityOverlaps {};
};

struct FAggroSearchComponent
{
	FVector Offset;
	TArray<FName> RequiredTags;
	ECollisionChannel CollisionChannel;
	float Radius;
};

struct FAggroRadius
{
	float Value { 0.0f };
};

struct FPotentialTargetActor
{
	TArray<AActor*> PotentialTargets;
};

struct FPotentialTargetEntities
{
	TArray<flecs::entity> PotentialTargets;
};

struct FTargetActor
{
	AActor* Value;
};

struct FEntityCustomTargetSorting
{
	TFunction<void(const TArray<AActor*>& ActorsToSort)> Sorter;
};

struct FFieldOfViewComponent
{
	float Value;
};

struct FTagPlayer {};
struct FTagReady {};
struct FTagHasTask {};
struct FTagFinishedPath {};