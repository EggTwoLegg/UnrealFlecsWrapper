#include "UECS/EcsPathfindingUtils.h"

#include "Detour/DetourCommon.h"
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"

#include "NavMesh/RecastHelpers.h"

bool FEcsPathfindingUtils::InitPathfinding(const FVector& UnrealStart, const FVector& UnrealEnd, const dtNavMeshQuery& Query,
	const dtQueryFilter* Filter, const FVector& NavExtent, FVector& RecastStart, dtPolyRef& StartPoly,
    FVector& RecastEnd, dtPolyRef& EndPoly)
{
	const FVector::FReal Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	const FVector RecastStartToProject = Unreal2RecastPoint(UnrealStart);
	const FVector RecastEndToProject = Unreal2RecastPoint(UnrealEnd);

	StartPoly = INVALID_NAVNODEREF;
	Query.findNearestPoly(&RecastStartToProject.X, Extent, Filter, &StartPoly, &RecastStart.X);
	if (StartPoly == INVALID_NAVNODEREF)
	{
		return false;
	}

	EndPoly = INVALID_NAVNODEREF;
	Query.findNearestPoly(&RecastEndToProject.X, Extent, Filter, &EndPoly, &RecastEnd.X);
	if (EndPoly == INVALID_NAVNODEREF)
	{
		if (Query.isRequiringNavigableEndLocation())
		{
			return false;
		}

		// we will use RecastEndToProject as the estimated end location since we didn't find a poly. It will be used to compute the heuristic mainly
		dtVcopy(&RecastEnd.X, &RecastEndToProject.X);
	}

	return true;
}

bool FEcsPathfindingUtils::FindPath(const dtNavMesh* NavMesh, const FVector& StartLoc, const FVector& EndLoc, const FVector::FReal CostLimit,
	const FVector& AgentExtents,
	const bool bRequireNavigableEndLocation,
	const dtQueryFilter* Filter,
	TArray<FVector>& PathPoints)
{
	dtNavMeshQuery NavQuery;
	NavQuery.init(NavMesh, 2048, nullptr);
	NavQuery.setRequireNavigableEndLocation(bRequireNavigableEndLocation);

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	FVector RecastStartPos, RecastEndPos;
	
	const bool bCanQuery = InitPathfinding(StartLoc, EndLoc, NavQuery, Filter, AgentExtents, RecastStartPos, StartPoly, RecastEndPos, EndPoly);
	if(!bCanQuery)
	{
		return false;
	}
	
	dtQueryResult PathResult;
	const dtStatus FindPathStatus = NavQuery.findPath(StartPoly, EndPoly, &RecastStartPos.X, &RecastEndPos.X, CostLimit, Filter, PathResult, 0);

	if (!dtStatusSucceed(FindPathStatus))
	{
		return true;
	}

	const int32 NumPathPoints = PathResult.size();
	
	PathPoints.Reserve(NumPathPoints);
	
	for (int32 IdxPathPoint = 0; IdxPathPoint < NumPathPoints; ++IdxPathPoint)
	{
		PathPoints.Emplace(Recast2UnrealPoint(PathResult.getPos(IdxPathPoint)));
	}

	return true;
}
