#pragma once

struct FNavmeshPathResponse
{
	TArray<FVector> Points;
	dtStatus Status;
	
	FORCEINLINE static int32 GetTypeId() { return EEcsComponentType::NavmeshPathResponse; }
};