#pragma once

#include "UECS/EntityPositionCache.h"
#include "UECS/flecs.h"
#include "UECS/Components/BaseComponents.h"

struct FOctreeNode
{
	static constexpr int32 MaxDepth { 4 };
	
    TArray<FEntityPositionCache> Entities;
    FBox Bounds;
    TStaticArray<FOctreeNode*, 8> Children;
	int64 Hash { 0 };
    bool IsLeaf { true };
	int32 Depth { 0 };
	int32 SubdivisionThreshold { 16 };

    FOctreeNode(const FBox& InBounds, const int32 InDepth, const int64 InHash) : Bounds(InBounds), Depth(InDepth), Hash(InHash) {}
	template<typename TSpatialHashMember>
	FOctreeNode* Add(const FVector& Position, flecs::entity& Entity)
    {
    	if(IsLeaf)
    	{
    		if(Entities.Num() >= SubdivisionThreshold && Depth < MaxDepth)
    		{
    			Subdivide();
    			FOctreeNode* ChildNode = GetChild(Position);
    			return ChildNode->Add<TSpatialHashMember>(Position, Entity);
    		}

    		TSpatialHashMember SpatialHashMember;
    		SpatialHashMember.Hash = Hash;
    		SpatialHashMember.IndexInArray = Entities.Num();
    		SpatialHashMember.OctreeNode = this;
    		Entity.set<TSpatialHashMember>(SpatialHashMember);
    		
    		Entities.Add(FEntityPositionCache { .Entity = Entity, .Position = Position });

    		return this;
    	}
    	
    	return GetChild(Position)->Add<TSpatialHashMember>(Position, Entity);
    }

	template<typename TSpatialHashMember>
	void Change(flecs::entity& Entity, const FVector& Position, TSpatialHashMember& SpatialHashMember)
    {
    	if(!IsLeaf) { return; }
    	
    	FOctreeNode* OldNode = SpatialHashMember.OctreeNode;
    	if(OldNode == this)
    	{
    		OldNode->Entities[SpatialHashMember.IndexInArray].Position = Position;
    		return;
    	}
    	
    	OldNode->Remove<TSpatialHashMember>(SpatialHashMember.IndexInArray);

    	FOctreeNode* NewNode = GetChild(Position);
    	NewNode->Add<TSpatialHashMember>(Position, Entity);
    }

	template<typename TSpatialHashMember>
	void Remove(const int32 IdxInArray)
    {
    	if(!IsLeaf) { return; }

    	const int32 NumEntities = Entities.Num();

    	if(NumEntities == 0 || IdxInArray >= NumEntities) { return; }
    	
    	// Find the entity at the end of the array and set its SpatialHashMember's IndexInArray to the removed entity's IndexInArray.
    	const FEntityPositionCache& LastEntityInArray = Entities[NumEntities - 1];
		if(!LastEntityInArray.Entity.is_valid() || !LastEntityInArray.Entity.is_alive()) { return; }
    	
    	TSpatialHashMember* LastEntitySpatialHashMember = LastEntityInArray.Entity.get_mut<TSpatialHashMember>();
    	LastEntitySpatialHashMember->IndexInArray = IdxInArray;
    	Entities[IdxInArray] = LastEntityInArray;
    	Entities.RemoveAt(NumEntities - 1, EAllowShrinking::No);
    }

    // Utility to determine child index based on position
    int32 GetChildIndex(const FVector& Position) const
	{
    	const FVector Center = Bounds.GetCenter();
        int32 IdxChild = 0;
    	
        if (Position.X > Center.X) { IdxChild |= 4; }
        if (Position.Y > Center.Y) { IdxChild |= 2; }
        if (Position.Z > Center.Z) { IdxChild |= 1; }

    	return IdxChild;
    }

	FOctreeNode* GetChild(const FVector& Position)
    {
    	if(IsLeaf) { return this; }
    	
	    return Children[GetChildIndex(Position)]->GetChild(Position);
    }

	void GetAllEntities(TArray<FEntityPositionCache>& OutEntities)
    {
	    if(IsLeaf)
	    {
		    OutEntities.Append(Entities);
	    	return;
	    }

    	for(int32 IdxChild = 0; IdxChild < 8; ++IdxChild)
    	{
    		Children[IdxChild]->GetAllEntities(OutEntities);
    	}
    }

	void GetEntitiesInBox(const FBox& Box, TArray<FEntityPositionCache>& OutEntities)
    {
	    if(IsLeaf)
	    {
		    for(const auto& Entity : Entities)
		    {
			    if(Box.IsInside(Entity.Position))
			    {
				    OutEntities.Add(Entity);
			    }
		    }
	    	return;
	    }

    	for(int32 IdxChild = 0; IdxChild < 8; ++IdxChild)
    	{
    		if(Children[IdxChild]->Bounds.Intersect(Box))
    		{
    			Children[IdxChild]->GetEntitiesInBox(Box, OutEntities);
    		}
    	}
    }

	void GetEntities(const FVector& Position, TArray<FEntityPositionCache>& OutEntities)
    {
    	if(IsLeaf)
    	{
    		OutEntities.Append(Entities);
    		return;
    	}
    	
    	GetChild(Position)->GetEntities(Position, OutEntities);
    }
	
    void Subdivide()
	{
        if (!IsLeaf) { return; }

    	IsLeaf = false;
        const FVector Size = Bounds.GetSize() * 0.5f;
        const FVector Min = Bounds.Min;

        for (int IdxChild = 0; IdxChild < 8; ++IdxChild)
        {
            FVector NewMin = Min;
            if (IdxChild & 4) { NewMin.X += Size.X; }
            if (IdxChild & 2) { NewMin.Y += Size.Y; }
            if (IdxChild & 1) { NewMin.Z += Size.Z; }
            FVector NewMax = NewMin + Size;
            Children[IdxChild] = new FOctreeNode(FBox(NewMin, NewMax), Depth + 1, Hash);
        }

        // Redistribute entities into children
        for (const auto& Entity : Entities)
        {
            const FVector EntityPosition = Entity.Position;
        	const int32 ChildIndex = GetChildIndex(EntityPosition);
        	Children[ChildIndex]->Entities.Add(Entity);
        }
    	
        Entities.Reset();
    }
	
    void TryCollapse()
	{
        if (IsLeaf) { return; }

    	TArray<FEntityPositionCache> AllEntities;

    	for (int IdxChild = 0; IdxChild < 8; ++IdxChild)
    	{
            if (!Children[IdxChild]->IsLeaf) { return; }
    		
            AllEntities.Append(Children[IdxChild]->Entities);
        }

    	Entities = AllEntities;
    	IsLeaf = true;
    }
};

struct FEntityGridHash
{
	TMap<int64, FOctreeNode*> Grid;
	float PartitionSize { 1600.0f };
	
	FORCEINLINE int64 GetGridKey(const FVector& Vector) const
	{
		const FIntVector2 GridCoords = GetGridCoords(Vector);
		return GetGridKey(GridCoords.X, GridCoords.Y);
	}

	FORCEINLINE static int64 GetGridKey(const int32 X, const int32 Y)
	{
		// Use uint64_t to avoid sign extension issues
		const uint64 A = static_cast<uint32>(X); // Cast a to uint32_t, then promote to uint64_t
		const uint64 B = static_cast<uint32>(Y); // Same for b
		return (A << 32) | B;
	}

	FORCEINLINE FIntVector2 GetGridCoords(const FVector& Vector) const
	{
		return FIntVector2(FMath::FloorToInt(Vector.X / PartitionSize), FMath::FloorToInt(Vector.Y / PartitionSize));
	}

	FORCEINLINE void GetEntitiesInRadius(const FVector& Position, const float Radius, TArray<flecs::entity>& OutEntities)
	{
		const FIntVector2 TopLeft = GetGridCoords(Position - FVector(Radius, Radius, 0.0f));
		const FIntVector2 BottomRight = GetGridCoords(Position + FVector(Radius, Radius, 0.0f));
		const float SquareRadius = Radius * Radius;
		
		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);
				
				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }

				const auto& Entities = (*OctreeNode)->GetChild(Position)->Entities;
				for(const auto& Cache : Entities)
				{
					const FVector EntityPosition = Cache.Position;
					const float SquareDistance = FVector::DistSquared(Position, EntityPosition);
					
					if(SquareDistance <= SquareRadius)
					{
						OutEntities.Add(Cache.Entity);
					}
				}
			}
		}
	}

	FORCEINLINE void GetEntitiesInRadiusFiltered(const FVector& Position, const float Radius, TArray<flecs::entity>& OutEntities, const TFunction<bool(const flecs::entity&)>& FilterFn)
	{
		const FIntVector2 TopLeft = GetGridCoords(Position - FVector(Radius, Radius, 0.0f));
		const FIntVector2 BottomRight = GetGridCoords(Position + FVector(Radius, Radius, 0.0f));
		const float SquareRadius = Radius * Radius;
		
		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);
				
				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }

				const auto& Entities = (*OctreeNode)->GetChild(Position)->Entities;
				for(const auto& Cache : Entities)
				{
					if(!FilterFn(Cache.Entity)) { continue; }
					
					const FVector EntityPosition = Cache.Position;
					const float SquareDistance = FVector::DistSquared(Position, EntityPosition);
					
					if(SquareDistance <= SquareRadius)
					{
						OutEntities.Add(Cache.Entity);
					}
				}
			}
		}
	}

	FORCEINLINE void GetEntitiesInBox(const flecs::entity& MyEntity, const FBox& Box, TArray<FEntityPositionCache>& OutEntities)
	{
		const FIntVector2 TopLeft = GetGridCoords(Box.Min);
		const FIntVector2 BottomRight = GetGridCoords(Box.Max);
		
		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);
				
				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }
				
				(*OctreeNode)->GetEntitiesInBox(Box, OutEntities);
			}
		}

		const int32 NumEntities = OutEntities.Num();
		for(int32 IdxEntity = NumEntities - 1; IdxEntity >= 0; --IdxEntity)
		{
			const FEntityPositionCache& Cache = OutEntities[IdxEntity];
			const FVector EntityPosition = Cache.Position;

			if(Cache.Entity == MyEntity || !Box.IsInside(EntityPosition))
			{
				OutEntities.RemoveAt(IdxEntity);
			}
		}
	}
	
	FORCEINLINE void GetEntitiesInBox(const FBox& Box, TArray<FEntityPositionCache>& OutEntities)
	{
		const FIntVector2 TopLeft = GetGridCoords(Box.Min);
		const FIntVector2 BottomRight = GetGridCoords(Box.Max);
		
		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);
				
				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }

				TArray<FEntityPositionCache> PotentialEntities;
				(*OctreeNode)->GetEntitiesInBox(Box, PotentialEntities);
				
				for(const auto& Cache : PotentialEntities)
				{
					const FVector EntityPosition = Cache.Position;
					
					if(Box.IsInside(EntityPosition))
					{
						OutEntities.Add(Cache);
					}
				}
			}
		}
	}

	FORCEINLINE void GetEntitiesInHemisphere(const flecs::entity& InEntity, const FVector& Position, const float Radius, const FVector& Normal, TArray<FEntityPositionCache>& OutEntities)
	{
		const float RadiusSquared = Radius * Radius;
		const FVector RadiusVec(Radius, Radius, Radius);
		const FBox SphereBox(Position - RadiusVec, Position + RadiusVec);
    
		const FIntVector2 TopLeft = GetGridCoords(SphereBox.Min);
		const FIntVector2 BottomRight = GetGridCoords(SphereBox.Max);

		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);

				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }
				
				(*OctreeNode)->GetEntitiesInBox(SphereBox, OutEntities);
			}
		}

		const int32 NumEntities = OutEntities.Num();
		for(int32 IdxEntity = NumEntities - 1; IdxEntity >= 0; --IdxEntity)
		{
			const FEntityPositionCache& Cache = OutEntities[IdxEntity];
			const FVector Direction = Cache.Position - Position;
			const float Distance = Direction.SquaredLength();

			if(Cache.Entity == InEntity
				|| Cache.Entity.parent() == InEntity
				|| Distance >= RadiusSquared || FVector::DotProduct(Direction, Normal) < 0)
			{
				OutEntities.RemoveAt(IdxEntity, 1, false);
			}
		}
	}

	FORCEINLINE void GetEntitiesInHemisphere(const FVector& Position, const float Radius, const FVector& Normal, TArray<FEntityPositionCache>& OutEntities)
	{
		const float RadiusSquared = Radius * Radius;
		const FVector RadiusVec(Radius, Radius, Radius);
		const FBox SphereBox(Position - RadiusVec, Position + RadiusVec);
    
		const FIntVector2 TopLeft = GetGridCoords(SphereBox.Min);
		const FIntVector2 BottomRight = GetGridCoords(SphereBox.Max);

		for(int32 Y = TopLeft.Y; Y <= BottomRight.Y; ++Y)
		{
			for(int32 X = TopLeft.X; X <= BottomRight.X; ++X)
			{
				const int64 Key = GetGridKey(X, Y);

				FOctreeNode** OctreeNode = Grid.Find(Key);
				if(nullptr == OctreeNode) { continue; }

				(*OctreeNode)->GetEntitiesInBox(SphereBox, OutEntities);
			}
		}

		const int32 NumEntities = OutEntities.Num();
		for(int32 IdxEntity = NumEntities - 1; IdxEntity >= 0; --IdxEntity)
		{
			const FEntityPositionCache& Cache = OutEntities[IdxEntity];
			const FVector Direction = Cache.Position - Position;
			const float Distance = Direction.SquaredLength();

			if(Distance >= RadiusSquared || FVector::DotProduct(Direction, Normal) < 0)
			{
				OutEntities.RemoveAt(IdxEntity, 1, false);
			}
		}
	}

	template<typename TSpatialHashMember>
	FORCEINLINE void Add(flecs::entity& Entity, const FPosition& Position)
	{
		const FVector EntityPosition = Position.Value;
		const int64 Key = GetGridKey(EntityPosition);
		
		FOctreeNode** OctreeNode = Grid.Find(Key);
		if(nullptr == OctreeNode)
		{
			const FVector BoundsMin = FVector(FMath::FloorToFloat(EntityPosition.X / PartitionSize) * PartitionSize, FMath::FloorToFloat(EntityPosition.Y / PartitionSize) * PartitionSize, 0.0f);
			const FVector BoundsMax = BoundsMin + FVector(PartitionSize, PartitionSize, 0.0f);

			FOctreeNode* NewNode = new FOctreeNode(FBox(BoundsMin, BoundsMax), 0, Key);
			Grid.Add(Key, NewNode);

			OctreeNode = Grid.Find(Key);
		}

		(*OctreeNode)->Add<TSpatialHashMember>(EntityPosition, Entity);
	}

	template<typename TSpatialHashMember>
	FORCEINLINE void Change(flecs::entity& Entity, const FPosition& Position, TSpatialHashMember& SpatialHashMember)
	{
		const FVector EntityPosition = Position.Value;
		const int64 Key = GetGridKey(EntityPosition);
		const int64 OldKey = SpatialHashMember.Hash;

		if(Key == OldKey)
		{
			FOctreeNode** OldOctreeRootNode = Grid.Find(OldKey);
			if(nullptr == OldOctreeRootNode)
			{
				const FVector BoundsMin = FVector(FMath::FloorToFloat(EntityPosition.X / PartitionSize) * PartitionSize, FMath::FloorToFloat(EntityPosition.Y / PartitionSize) * PartitionSize, 0.0f);
				const FVector BoundsMax = BoundsMin + FVector(PartitionSize, PartitionSize, 0.0f);

				FOctreeNode* NewNode = new FOctreeNode(FBox(BoundsMin, BoundsMax), 0, OldKey);
				Grid.Add(OldKey, NewNode);
				OldOctreeRootNode = Grid.Find(OldKey);
			}

			FOctreeNode* OldOctreeNode = (*OldOctreeRootNode)->GetChild(EntityPosition);
			
			OldOctreeNode->Change<TSpatialHashMember>(Entity, EntityPosition, SpatialHashMember);
			return;
		}

		FOctreeNode** NewOctreeRootNode = Grid.Find(Key);
		if(nullptr == NewOctreeRootNode)
		{
			const FVector BoundsMin = FVector(FMath::FloorToInt(EntityPosition.X / PartitionSize) * PartitionSize, FMath::FloorToInt(EntityPosition.Y / PartitionSize) * PartitionSize, FMath::FloorToInt(EntityPosition.Z / PartitionSize) * PartitionSize);
			const FVector BoundsMax = BoundsMin + FVector(PartitionSize);

			FOctreeNode* NewNode = new FOctreeNode(FBox(BoundsMin, BoundsMax), 0, Key);
			Grid.Add(Key, NewNode);
			NewOctreeRootNode = Grid.Find(Key);
		}

		SpatialHashMember.OctreeNode->Remove<TSpatialHashMember>(SpatialHashMember.IndexInArray);
		// if(SpatialHashMember.OctreeNode->Entities.Num() <= 0)
		// {
		// 	FOctreeNode** Node = Grid.Find(OldKey);
		// 	if(nullptr != Node && SpatialHashMember.OctreeNode == *Node)
		// 	{
		// 		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Remove Old Node"));
		// 		Grid.Remove(Key);
		// 	}
		// }

		FOctreeNode* NewOctreeNode = (*NewOctreeRootNode)->GetChild(EntityPosition);
		NewOctreeNode->Add<TSpatialHashMember>(EntityPosition, Entity);
	}

	template<typename TSpatialHashMember>
	FORCEINLINE void Remove(const TSpatialHashMember& Member)
	{
		const int64 Key = Member.Hash;

		FOctreeNode* OctreeNode = Member.OctreeNode;
		if(nullptr == OctreeNode) { return; }

		OctreeNode->Remove<TSpatialHashMember>(Member.IndexInArray);

		if(OctreeNode->Entities.Num() <= 0)
		{
			FOctreeNode** Node = Grid.Find(Key);
			if(nullptr != Node && OctreeNode == *Node)
			{
				Grid.Remove(Key);
			}
		}
	}

	void DrawBounds(UWorld* World)
	{
		for(auto& KVP : Grid)
		{
			DrawBounds(World, KVP.Value);
		}
	}

	static void DrawBounds(UWorld* World, const FOctreeNode* Node)
	{
		if(Node->IsLeaf)
		{
			DrawDebugBox(World, Node->Bounds.GetCenter(), Node->Bounds.GetExtent(), FColor::Green, false, 0.0f, 0, 5.0f);
		}
		else
		{
			for(const FOctreeNode* SubNode : Node->Children)
			{
				DrawBounds(World, SubNode);
			}
		}
	}
};