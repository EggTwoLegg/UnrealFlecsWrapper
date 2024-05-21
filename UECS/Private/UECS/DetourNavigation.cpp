#include "UECS/DetourNavigation.h"

#include "Detour/DetourNavMeshQuery.h"
#include "NavMesh/PImplRecastNavMesh.h"

FDetourNavigation::FDetourNavigation(dtNavMesh* DetourMesh)
{
	NavMesh = (DetourMesh);
	NavQuery = new dtNavMeshQuery();
	NavQuery->init(NavMesh, 2048);
	
	DefaultFilter.setIncludeFlags(0xffff);
	DefaultFilter.setExcludeFlags(0);
}

FDetourNavigation::~FDetourNavigation()
{
	delete NavQuery;
}

dtStatus FDetourNavigation::FindPath(const FVector& Start, const FVector& End, const FVector& ActorExtents, TArray<FVector>& PathPoints) const
{
	const FVector RecastStart = Unreal2RecastPoint(Start);
	const FVector RecastEnd = Unreal2RecastPoint(End);
	
	dtPolyRef StartRef, EndRef;
	NavQuery->findNearestPoly(&RecastStart.X, &ActorExtents.X, &DefaultFilter, &StartRef, nullptr);
	NavQuery->findNearestPoly(&RecastEnd.X, &ActorExtents.X, &DefaultFilter, &EndRef, nullptr);

	if (StartRef == 0 || EndRef == 0)
	{
		return DT_FAILURE;
	}

	dtQueryResult QueryResult;
	const dtStatus Status = NavQuery->findPath(StartRef, EndRef, &Start.X, &End.X, 0.0f, &DefaultFilter, QueryResult, nullptr);

	const int32 NumPoints = QueryResult.size();
	if (NumPoints <= 0) { return Status; }
	
	PathPoints.Reset();
	for (int32 IdxPoint = 0; IdxPoint < NumPoints; ++IdxPoint)
	{
		PathPoints.Emplace(Recast2UnrealPoint(QueryResult.getPos(IdxPoint)));
	}

	return Status;
}

bool FDetourNavigation::Raycast(const FVector& Start, const FVector& End, const FVector& ActorExtents, FVector& HitNormal, double& HitTime) const
{
	const FVector RecastStart = Unreal2RecastPoint(Start);
	const FVector RecastEnd = Unreal2RecastPoint(End);
	
	dtPolyRef StartRef, EndRef;
	NavQuery->findNearestPoly(&RecastStart.X, &ActorExtents.X, &DefaultFilter, &StartRef, nullptr);
	NavQuery->findNearestPoly(&RecastEnd.X, &ActorExtents.X, &DefaultFilter, &EndRef, nullptr);
	
	if (StartRef == 0 || EndRef == 0) { return false; }

	FVector ProjectedStart, ProjectedEnd;
	const dtStatus StatusA = NavQuery->projectedPointOnPoly(StartRef, &RecastStart.X, &ProjectedStart.X);
	const dtStatus StatusB = NavQuery->projectedPointOnPoly(EndRef, &RecastEnd.X, &ProjectedEnd.X);

	if(StatusA != DT_SUCCESS || StatusB != DT_SUCCESS) { return false; }
	
	static constexpr int MAX_RES = 32;
	
	dtPolyRef ResultPolyPath[MAX_RES];
	int NumResultPolyPath = 0;

	NavQuery->raycast(StartRef, &ProjectedStart.X, &ProjectedEnd.X, &DefaultFilter, &HitTime, &HitNormal.X, ResultPolyPath, &NumResultPolyPath, MAX_RES);

	return FMath::IsNearlyEqual(HitTime, DT_REAL_MAX, KINDA_SMALL_NUMBER) == false;
}
