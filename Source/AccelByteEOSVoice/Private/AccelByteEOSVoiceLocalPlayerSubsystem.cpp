#include "AccelByteEOSVoiceLocalPlayerSubsystem.h"

#include "AccelByteEOSVoice.h"
#include "AccelByteEOSVoiceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

bool UAccelByteEOSVoiceLocalPlayerSubsystem::JoinChannel(const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate)
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->JoinChannel(LocalUserNum, ChannelName, ChannelCredentials, ChannelType, Delegate);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to join channel %s. Local player or voice subsystem not ready."), *ChannelName);
	return false;
}

bool UAccelByteEOSVoiceLocalPlayerSubsystem::LeaveChannel(const FString& ChannelName, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate)
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->LeaveChannel(LocalUserNum, ChannelName, Delegate);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to leave channel %s. Local player or voice subsystem not ready."), *ChannelName);
	return false;
}

bool UAccelByteEOSVoiceLocalPlayerSubsystem::SetPlayerMuted(const FString& PlayerName, bool bIsMuted)
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->SetPlayerMuted(LocalUserNum, PlayerName, bIsMuted);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change mute state for %s. Local player or voice subsystem not ready."), *PlayerName);
	return false;
}

bool UAccelByteEOSVoiceLocalPlayerSubsystem::SetAudioInputDeviceMuted(bool bIsMuted)
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->SetAudioInputDeviceMuted(LocalUserNum, bIsMuted);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change input mute. Local player or voice subsystem not ready."));
	return false;
}

bool UAccelByteEOSVoiceLocalPlayerSubsystem::SetAudioOutputDeviceMuted(bool bIsMuted)
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->SetAudioOutputDeviceMuted(LocalUserNum, bIsMuted);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change output mute. Local player or voice subsystem not ready."));
	return false;
}

bool UAccelByteEOSVoiceLocalPlayerSubsystem::TransmitToSpecificChannel(ETransmitChannel Channel) const
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			FString ChannelName;
			switch(Channel)
			{
			case ETransmitChannel::Game:
				ChannelName = EOS_VOICE_GAME_SESSION_CHANNEL;
				break;
			case ETransmitChannel::Team:
				ChannelName = EOS_VOICE_TEAM_SESSION_CHANNEL;
				break;
			case ETransmitChannel::Party:
				ChannelName = EOS_VOICE_PARTY_SESSION_CHANNEL;
				break;
			default:
				ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change transmition channel. Channel name unknown"));
				return false;
			}
			return VoiceSubsystem->TransmitToSpecificChannel(LocalUserNum, ChannelName);
		}
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to transmit to channel. Local player or voice subsystem not ready."));
	return false;
}

IVoiceChatUser* UAccelByteEOSVoiceLocalPlayerSubsystem::GetVoiceChatUser() const
{
	if (UAccelByteEOSVoiceSubsystem* VoiceSubsystem = GetVoiceSubsystem())
	{
		const int32 LocalUserNum = GetOwningLocalUserNum();
		if (LocalUserNum != INDEX_NONE)
		{
			return VoiceSubsystem->GetVoiceChatUser(LocalUserNum);
		}
	}
	
	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to get VoiceChatUser."));
	return nullptr;
}

int32 UAccelByteEOSVoiceLocalPlayerSubsystem::GetOwningLocalUserNum() const
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		return LocalPlayer->GetControllerId();
	}

	return INDEX_NONE;
}

UAccelByteEOSVoiceSubsystem* UAccelByteEOSVoiceLocalPlayerSubsystem::GetVoiceSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
	{
		return GameInstance->GetSubsystem<UAccelByteEOSVoiceSubsystem>();
	}

	return nullptr;
}