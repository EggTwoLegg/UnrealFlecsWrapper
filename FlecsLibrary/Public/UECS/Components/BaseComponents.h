#pragma once
#include "NavigationSystemTypes.h"

struct FActorReference
{
	AActor* actor;
};

struct FPosition
{
	FVector value;
};

struct FRotationComponent
{
	FQuat value = FQuat::Identity; 
};

struct FScale
{
	FVector value;
};

struct FVelocity
{
	FVector value;

	void Add(FVector otherVelocity) { value += otherVelocity; }
};

struct FSpeed
{
	float max;
	float linearAcceleration;
};

struct FOneFrameMovementSequence
{
	TArray<FVector, TInlineAllocator<8>> steps;
};

struct FRadius
{
	float value;
};

struct FHealth
{
	float maxHealth;
	float currentHealth;
};

struct FInstancedStaticMesh
{
	UInstancedStaticMeshComponent* ismComp;
	int32 numInstances;

	TMap<int32, TArray<flecs::entity>> entityOverlaps;
	
	TArray<FTransform> addedTransforms;
	TArray<FTransform> updatedTransforms;
	TArray<int32>      removedIndices;
};

struct FTransformComponent
{
	FTransform value; 
};

struct FNavmeshAgent
{
	FNavAgentProperties agentProperties;
};

struct FNavmeshPathRequest
{
	FVector startLocation;
	FVector endLocation;
};

struct FNavMeshPath
{
	TArray<FNavPathPoint, TInlineAllocator<8>> path;
	int32 idxPoint;
};

struct FCollisionShapeComponent
{
	FCollisionShape shape;
	FVector offset;
	ECollisionChannel collisionChannel;
};

struct FOverlapComponent
{
	TArray<flecs::entity> entityOverlaps;
};

struct FTagPlayer { };