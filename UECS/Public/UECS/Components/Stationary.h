#pragma once

#include "UECS/EcsComponentType.h"

struct FStationary
{
	FORCEINLINE static int32 GetType() { return EEcsComponentType::Stationary; }
};
