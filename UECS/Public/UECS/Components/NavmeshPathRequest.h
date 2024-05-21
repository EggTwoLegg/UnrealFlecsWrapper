#pragma once

struct FNavmeshPathRequest
{
	FVector Start;
	FVector End;
	FORCEINLINE static int32 GetTypeId()
	{
		return EEcsComponentType::NavmeshPathRequest;
	}
};