#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAccelByteEOSVoice, Log, All);

#define ACCELBYTE_EOS_VOICE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAccelByteEOSVoice, Verbosity, TEXT("%s: ") Format, ANSI_TO_TCHAR(__FUNCTION__), ##__VA_ARGS__)

class FAccelByteEOSVoiceModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
