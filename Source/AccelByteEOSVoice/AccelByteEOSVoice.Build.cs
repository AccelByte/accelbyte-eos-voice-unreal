// Copyright (c) 2026 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

using UnrealBuildTool;

public class AccelByteEOSVoice : ModuleRules
{
    public AccelByteEOSVoice(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "EOSSDK",
                "EOSShared",
                "OnlineSubsystemAccelByte",
                "OnlineSubsystem",
                "AccelByteUe4Sdk",
                "AccelByteUe4SdkCustomization",
                "OnlineSubsystemEOS",
                "VoiceChat",
                "EOSVoiceChat",
                "Json",
                "JsonUtilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "OnlineSubsystemUtils",
            }
        );
    }
}