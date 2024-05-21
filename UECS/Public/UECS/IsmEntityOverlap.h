#pragma once
#include "flecs.h"

struct FIsmEntityOverlap
{
	int32 IsmInstanceId { 0 };
	flecs::entity EntityOverlapped;
};
