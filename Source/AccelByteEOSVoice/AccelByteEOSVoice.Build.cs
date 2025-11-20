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
                "OnlineSubsystemAccelByte",
                "OnlineSubsystem",
                "AccelByteUe4Sdk",
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