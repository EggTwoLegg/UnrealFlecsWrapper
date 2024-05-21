#pragma once

#include "flecs.h"

struct FEntityPositionCache
{
	flecs::entity Entity;
	FVector Position;
};
