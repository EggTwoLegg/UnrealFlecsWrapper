#pragma once

#include "UECS/UnrealEcsSystem.h"

struct SystemEndBarrier : public UnrealEcsSystem
{
	virtual void schedule(UnrealEcsSystemScheduler* inScheduler) override;	
};
