#pragma once

#include "Detour/DetourNavMesh.h"

struct dtQueryResult;
class dtQueryFilter;
class dtNavMeshQuery;

struct FLECSLIBRARY_API FEcsPathfindingUtils
{
	static bool InitPathfinding(const FVector& UnrealStart, const FVector& UnrealEnd,
	                            const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
	                            const FVector& NavExtent, FVector& RecastStart,
	                            dtPolyRef& StartPoly, FVector& RecastEnd, dtPolyRef& EndPoly);

	static bool FindPath(const dtNavMesh* NavMesh, const FVector& StartLoc, const FVector& EndLoc, const FVector::FReal CostLimit, const FVector& AgentExtents, const bool bRequireNavigableEndLocation, const dtQueryFilter* Filter, TArray
	                     <FVector>& PathPoints);
}; 