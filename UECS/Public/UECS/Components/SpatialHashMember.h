#pragma once

struct FGridMember
{
	int64 Hash;
	int32 IndexInArray { -1 };
	struct FOctreeNode* OctreeNode { nullptr };
};