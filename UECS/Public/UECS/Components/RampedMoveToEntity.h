#pragma once

#include "UECS/flecs.h"

struct FRampedMoveToEntity
{
	flecs::entity Target;
	float RampTime { 0.0f };
	float RampMaxTime { 1.0f };
	float MaxSpeed { 1000.0f };
};
