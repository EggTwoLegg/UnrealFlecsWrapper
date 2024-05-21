#pragma once

struct FEcsSystemTickDebt
{
	float TickInterval { 1.0f / 60.0f };
	float Debt { 0.0f };
	int32 NumTicks { 0 };

	FORCEINLINE void Reset()
	{
		Debt = 0.0f;
		NumTicks = 0;
	}

	FORCEINLINE bool AddDebt(const float InDebt, const int32 TargetNumTicks)
	{
		Debt += InDebt;
		const int32 NumTicksToAdd = FMath::FloorToInt(Debt / TickInterval);
		NumTicks += NumTicksToAdd;
		Debt -= NumTicksToAdd * TickInterval;

		return (NumTicks / TargetNumTicks) > 0;
	}
};
