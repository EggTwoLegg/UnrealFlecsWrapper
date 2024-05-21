#pragma once
#include "UECS/flecs.h"

struct FCollisionResults
{
	bool bHit { false };
	float TimeOfImpact { 0.0f };
	flecs::entity HitEntity;
};
