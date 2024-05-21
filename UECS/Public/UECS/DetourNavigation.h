#pragma once

#include "CoreMinimal.h"

#include "Detour/DetourNavMeshQuery.h"

class dtNavMeshQuery;

class FLECSLIBRARY_API FDetourNavigation
{
private:
	dtNavMesh* NavMesh;
	dtNavMeshQuery* NavQuery;
	dtQueryFilter DefaultFilter;

public:
	FDetourNavigation(dtNavMesh* DetourMesh);
	~FDetourNavigation();
	
	dtStatus FindPath(const FVector& Start, const FVector& End, const FVector& ActorExtents, TArray<FVector>& PathPoints) const;
	bool Raycast(const FVector& Start, const FVector& End, const FVector& ActorExtents, FVector& HitNormal, double& HitTime) const;
};
