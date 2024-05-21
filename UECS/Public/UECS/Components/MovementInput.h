#pragma once

struct FMovementInput
{
	FVector Input;
	
	FORCEINLINE static int32 GetTypeId()
	{
		return EEcsComponentType::MovementInput;
	}
};