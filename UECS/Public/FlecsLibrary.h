#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

// DECLARE_LOG_CATEGORY_EXTERN(PeGeneral, All, All);

DECLARE_STATS_GROUP(TEXT("ECS"), STATGROUP_ECS, STATCAT_ECS);

class FFlecsLibrary : public IModuleInterface 
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};