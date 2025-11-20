#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "VoiceChat.h"
#include "AccelByteEOSVoiceLocalPlayerSubsystem.generated.h"

class UAccelByteEOSVoiceSubsystem;

/**
 * Local player level helper to route voice chat requests through the AccelByte voice subsystem.
 */
UCLASS()
class ACCELBYTEEOSVOICE_API UAccelByteEOSVoiceLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	bool JoinChannel(const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate = FOnVoiceChatChannelJoinCompleteDelegate());
	bool LeaveChannel(const FString& ChannelName, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate = FOnVoiceChatChannelLeaveCompleteDelegate());
	bool SetPlayerMuted(const FString& PlayerName, bool bIsMuted);
	bool SetAudioInputDeviceMuted(bool bIsMuted);
	bool SetAudioOutputDeviceMuted(bool bIsMuted);

	enum class ETransmitChannel : uint8
	{
		Party,
		Team,
		Game
	};
	bool TransmitToSpecificChannel(ETransmitChannel Channel) const;
	
	IVoiceChatUser* GetVoiceChatUser() const;

private:
	int32 GetOwningLocalUserNum() const;
	UAccelByteEOSVoiceSubsystem* GetVoiceSubsystem() const;
};
