#pragma once

#include "UECS/EcsComponentType.h"

struct FHealth
{
	float MaxHealth;
	float CurrentHealth;

	FORCEINLINE static constexpr int32 GetTypeId()
	{
		return EEcsComponentType::Health;
	}
};
