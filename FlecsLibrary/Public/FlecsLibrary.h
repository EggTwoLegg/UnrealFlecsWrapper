#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

// DECLARE_LOG_CATEGORY_EXTERN(PeGeneral, All, All);

class FFlecsLibrary : public IModuleInterface 
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};