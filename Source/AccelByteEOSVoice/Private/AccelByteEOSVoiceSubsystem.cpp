// Fill out your copyright notice in the Description page of Project Settings.


#include "AccelByteEOSVoiceSubsystem.h"

#include "AccelByteEOSVoice.h"
#include "EOSVoiceChatTypes.h"
#include "IOnlineSubsystemEOS.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemAccelByteDefines.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "VoiceChat.h"

#if UE_BUILD_DEVELOPMENT
static FAutoConsoleCommandWithWorldAndArgs GSetTransmitChannel(
	TEXT("AB.EOSVoice.TransmitTo"),  TEXT("Set transmision to channel party-session or game-session"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if(Args.Num() > 0)
		{
			World->GetGameInstance()->GetSubsystem<UAccelByteEOSVoiceSubsystem>()->TransmitToSpecificChannel(0, Args[0]);
		}
	})
);

static FAutoConsoleCommandWithWorldAndArgs GSetMute(
	TEXT("AB.EOSVoice.SetMute"),  TEXT("Set true to muted"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if(Args.Num() > 0)
		{
			bool Muted = FCString::ToBool(*Args[0]);
			World->GetGameInstance()->GetSubsystem<UAccelByteEOSVoiceSubsystem>()->SetAudioInputDeviceMuted(0, Muted);
		}
	})
);

static FAutoConsoleCommandWithWorldAndArgs GSetDeafen(
	TEXT("AB.EOSVoice.SetDeafen"),  TEXT("Set true to deafen"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if(Args.Num() > 0)
		{
			bool Muted = FCString::ToBool(*Args[0]);
			World->GetGameInstance()->GetSubsystem<UAccelByteEOSVoiceSubsystem>()->SetAudioOutputDeviceMuted(0, Muted);
		}
	})
);

static FAutoConsoleCommandWithWorld GListPlayers(
	TEXT("AB.EOSVoice.ListPlayers"), TEXT("List players registered in voice chat"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		IVoiceChatUser* VoiceChat = World->GetGameInstance()->GetSubsystem<UAccelByteEOSVoiceSubsystem>()->GetVoiceChatUser(0);
		if(VoiceChat)
		{
			TArray<FString> Channels = VoiceChat->GetChannels();
			for(const FString& Channel : Channels)
			{
				ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Channel: %s"), *Channel);
				for(const FString& Player : VoiceChat->GetPlayersInChannel(Channel))
				{
					ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Player: %s"), *Player);
				}
				ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("==========================================="));
			}
		}
	})
);
#endif

void UAccelByteEOSVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), ACCELBYTE_SUBSYSTEM);
	check(Subsystem)

	IdentityAccelByte = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	check(IdentityAccelByte)

	IOnlineSessionPtr SessionAccelByte = Subsystem->GetSessionInterface();
	check(SessionAccelByte)

	IOnlineSubsystemEOS* EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld(), EOS_SUBSYSTEM));
	check(EOSSubsystem)

	IdentityEOS = EOSSubsystem->GetIdentityInterface();
	check(IdentityEOS)

	IdentityAccelByte->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnAccelByteLoginCompleted));
	SessionAccelByte->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnSessionDestroyed));
	IdentityEOS->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnEpicLoginCompleted));
}

void UAccelByteEOSVoiceSubsystem::Deinitialize()
{
	Super::Deinitialize();

	IdentityEOS->Logout(0);
}

IVoiceChatUser* UAccelByteEOSVoiceSubsystem::GetVoiceChatUser(int32 LocalUserNum) const
{
	if (const FAccelByteVoiceLocalUser* VoiceUser = FindVoiceLocalUser(LocalUserNum))
	{
		return VoiceUser->VoiceChatUser;
	}

	return nullptr;
}

bool UAccelByteEOSVoiceSubsystem::JoinChannel(int32 LocalUserNum, const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
		VoiceUser->JoinChannel(ChannelName, ChannelCredentials, ChannelType, Delegate);
		return true;
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to join channel %s. Voice user not ready for LocalUserNum %d."), *ChannelName, LocalUserNum);
	return false;
}

bool UAccelByteEOSVoiceSubsystem::LeaveChannel(int32 LocalUserNum, const FString& ChannelName, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
		VoiceUser->LeaveChannel(ChannelName, Delegate);
		return true;
	}

	ACCELBYTE_EOS_VOICE_LOG(Verbose, TEXT("Unable to leave channel %s. Voice user not ready for LocalUserNum %d."), *ChannelName, LocalUserNum);
	return false;
}

bool UAccelByteEOSVoiceSubsystem::SetPlayerMuted(int32 LocalUserNum, const FString& PlayerName, bool bIsMuted)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
		VoiceUser->SetPlayerMuted(PlayerName, bIsMuted);
		return true;
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to update mute state for player %s. Voice user not ready for LocalUserNum %d."), *PlayerName, LocalUserNum);
	return false;
}

bool UAccelByteEOSVoiceSubsystem::SetAudioInputDeviceMuted(int32 LocalUserNum, bool bIsMuted)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
		VoiceUser->SetAudioInputDeviceMuted(bIsMuted);
		return true;
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change input mute. Voice user not ready for LocalUserNum %d."), LocalUserNum);
	return false;
}

bool UAccelByteEOSVoiceSubsystem::SetAudioOutputDeviceMuted(int32 LocalUserNum, bool bIsMuted)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
		VoiceUser->SetAudioOutputDeviceMuted(bIsMuted);
		return true;
	}

	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to change output mute. Voice user not ready for LocalUserNum %d."), LocalUserNum);
	return false;
}

bool UAccelByteEOSVoiceSubsystem::TransmitToSpecificChannel(int32 LocalUserNum, const FString& ChannelName)
{
	if (IVoiceChatUser* VoiceUser = GetVoiceChatUser(LocalUserNum))
	{
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)
		VoiceUser->TransmitToSpecificChannels({ChannelName});
		return true;
#else
		VoiceUser->TransmitToSpecificChannel(ChannelName);
		return true;
#endif
	}
	ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to transmit to channel %s. Voice user not ready for LocalUserNum %d."), *ChannelName, LocalUserNum);
	return false;
}

void UAccelByteEOSVoiceSubsystem::LoginToEpic(int32 LocalUserNum)
{
	ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Start login to EOS voice for LocalUserNum %d"), LocalUserNum);
	IOnlineSubsystemEOS* EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld(), EOS_SUBSYSTEM));
	if (EOSSubsystem == nullptr)
	{
		ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("EOS subsystem is not available, cannot login voice user %d."), LocalUserNum);
		return;
	}

	FOnlineAccountCredentials EpicCreds;
	EpicCreds.Type = TEXT("AccelByte:OpenIdAccessToken");
	EpicCreds.Token = IdentityAccelByte->GetAuthToken(LocalUserNum);

	EOSSubsystem->GetIdentityInterface()->Login(LocalUserNum, EpicCreds);
}

void UAccelByteEOSVoiceSubsystem::OnAccelByteLoginCompleted(int LocalUserNum, bool bSuccessful, const FUniqueNetId& UniqueNetId, const FString& Error)
{
	if (!bSuccessful)
	{
		ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("AccelByte login failed for LocalUserNum %d: %s"), LocalUserNum, *Error);
		return;
	}

	check(Online::GetSubsystem(GetWorld(), ACCELBYTE_SUBSYSTEM));

	FAccelByteVoiceLocalUser& VoiceUser = LocalVoiceUsers.FindOrAdd(LocalUserNum);
	VoiceUser.ApiClient = IdentityAccelByte->GetApiClient(LocalUserNum);
	if (!VoiceUser.ApiClient.IsValid())
	{
		ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("API client is invalid for LocalUserNum %d"), LocalUserNum);
		return;
	}

	// Register lobby freeform notif
	VoiceUser.LobbyMessageDelegateHandle = VoiceUser.ApiClient->GetLobbyApi().Pin()->AddMessageNotifDelegate(AccelByte::Api::Lobby::FMessageNotif::CreateUObject(this, &ThisClass::OnVoiceTokenReceived, LocalUserNum));

	VoiceUser.ApiClient->GetUserApi().Pin()->GetData(THandler<FAccountUserData>::CreateUObject(this, &ThisClass::OnGetUserData, LocalUserNum),
		FErrorHandler::CreateWeakLambda(this, [LocalUserNum](int32 ErrCode, const FString& ErrMsg)
		{
			ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Unable to gather user data for LocalUserNum %d! [%d] %s"), LocalUserNum, ErrCode, *ErrMsg);
		})
	);
}

void UAccelByteEOSVoiceSubsystem::OnEpicLoginCompleted(int LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueNetId, const FString& Error)
{
	if (!bWasSuccessful)
	{
		ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Failed to login to EOS voice for LocalUserNum %d: %s"), LocalUserNum, *Error);
		return;
	}
	
	IOnlineSubsystemEOS* EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld(), EOS_SUBSYSTEM));
	check(EOSSubsystem)

	if (FAccelByteVoiceLocalUser* VoiceUser = FindVoiceLocalUser(LocalUserNum))
	{
		VoiceUser->VoiceChatUser = EOSSubsystem->GetVoiceChatUserInterface(UniqueNetId);
		VoiceUser->VoiceChatUser->SetAudioInputDeviceMuted(false);
	}

	ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("EOS voice user logged in: %s"), *UniqueNetId.ToDebugString());
}

void UAccelByteEOSVoiceSubsystem::OnGetUserData(const FAccountUserData& AccountUserData, int32 LocalUserNum)
{
	if(!AccountUserData.DisplayName.IsEmpty())
	{
		LoginToEpic(LocalUserNum);
		return;
	}
	
	const UAccelByteEOSVoiceConfig* VoiceConfig = GetDefault<UAccelByteEOSVoiceConfig>();
	if (!VoiceConfig)
	{
		ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Unable to load UAccelByteEOSVoiceConfig, abort login to Epic."));
		return;
	}

	if(!VoiceConfig->bAutoFillEmptyDisplayName)
	{
		ACCELBYTE_EOS_VOICE_LOG(Error,
			TEXT("Display Name is empty and bAutoFillEmptyDisplayName is set to false. Suggestion: \n1. Create a full account, OR \n2. Enable bAutoFillEmptyDisplayName to auto generate DisplayName and UniqueDisplayName"));
		return;
	}

	FAccelByteVoiceLocalUser* VoiceUser = FindVoiceLocalUser(LocalUserNum);
	if (VoiceUser == nullptr || !VoiceUser->ApiClient.IsValid())
	{
		ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("API client unavailable while updating display name for LocalUserNum %d"), LocalUserNum);
		return;
	}

	// In EOS Open ID, Display Name must be filled. This will auto generate Display Name with format Player_UID
	FUserUpdateRequest UpdateRequest;
	UpdateRequest.DisplayName = FString::Printf(TEXT("Player-%s"), *AccountUserData.UserId.Left(4));
	UpdateRequest.UniqueDisplayName = FString::Printf(TEXT("%s-%04d"), *AccountUserData.UserId.Left(7), FMath::RandRange(0, 9999));
	VoiceUser->ApiClient->GetUserApi().Pin()->UpdateUser(
		UpdateRequest, 
		THandler<FAccountUserData>::CreateUObject(this, &ThisClass::OnUpdateDisplayNameCompleted, LocalUserNum),
		FErrorHandler::CreateWeakLambda(this, [LocalUserNum](int32 ErrCode, const FString& ErrMsg)
	{
		ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Unable to update user data for LocalUserNum %d! [%d] %s"), LocalUserNum, ErrCode, *ErrMsg);
	}));
}

void UAccelByteEOSVoiceSubsystem::OnUpdateDisplayNameCompleted(const FAccountUserData& AccountUserData, int32 LocalUserNum)
{
	LoginToEpic(LocalUserNum);
}

void UAccelByteEOSVoiceSubsystem::OnVoiceTokenReceived(FAccelByteModelsNotificationMessage const& AccelByteModelsNotificationMessage, int32 LocalUserNum)
{
	if(!AccelByteModelsNotificationMessage.Topic.Equals(EOS_VOICE_TOKEN_TOPIC))
	{
		return;
	}

	if(GetVoiceChatUser(LocalUserNum) == nullptr)
	{
		ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("VoiceChatUser is nullptr! Unable to join voice chat for LocalUserNum %d"), LocalUserNum);
		return;
	}

	FVoiceChatJoinToken JoinToken;
	FJsonObjectConverter::JsonObjectStringToUStruct(AccelByteModelsNotificationMessage.Payload, &JoinToken);

	if (!JoinToken.Type.Equals(EOS_VOICE_GAME_SESSION_CHANNEL)
		&& !JoinToken.Type.Equals(EOS_VOICE_PARTY_SESSION_CHANNEL)
		&& !JoinToken.Type.Equals(EOS_VOICE_TEAM_SESSION_CHANNEL))
	{
		ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Type: %s is incorrect. Expected to be %s or %s or %s"), *JoinToken.Type, EOS_VOICE_GAME_SESSION_CHANNEL, EOS_VOICE_PARTY_SESSION_CHANNEL, EOS_VOICE_TEAM_SESSION_CHANNEL);
		return;
	}
	
	FEOSVoiceChatChannelCredentials Credentials;
	Credentials.ClientBaseUrl = JoinToken.ClientBaseUrl;
	Credentials.ParticipantToken = JoinToken.Token;
	JoinChannel(
		LocalUserNum,
		JoinToken.Type,
		Credentials.ToJson(),
		EVoiceChatChannelType::NonPositional,
			FOnVoiceChatChannelJoinCompleteDelegate::CreateWeakLambda(this, [](const FString& ChannelName, const FVoiceChatResult& Result)
			{
				ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Joined voice chat channel %s"), *ChannelName);
			})
	);
}

void UAccelByteEOSVoiceSubsystem::OnSessionDestroyed(FName SessionName, bool bWasSuccess)
{
	if(!bWasSuccess)
	{
		return;
	}

	for (const TPair<int32, FAccelByteVoiceLocalUser>& Entry : LocalVoiceUsers)
	{
		const int32 LocalUserNum = Entry.Key;

		if(SessionName.IsEqual(NAME_GameSession))
		{
			LeaveChannel(LocalUserNum, EOS_VOICE_GAME_SESSION_CHANNEL);
			LeaveChannel(LocalUserNum, EOS_VOICE_TEAM_SESSION_CHANNEL);
		}
		else if(SessionName.IsEqual(NAME_PartySession))
		{
			LeaveChannel(LocalUserNum, EOS_VOICE_PARTY_SESSION_CHANNEL);
		}
	}
}

UAccelByteEOSVoiceSubsystem::FAccelByteVoiceLocalUser* UAccelByteEOSVoiceSubsystem::FindVoiceLocalUser(int32 LocalUserNum)
{
	return LocalVoiceUsers.Find(LocalUserNum);
}

const UAccelByteEOSVoiceSubsystem::FAccelByteVoiceLocalUser* UAccelByteEOSVoiceSubsystem::FindVoiceLocalUser(int32 LocalUserNum) const
{
	return LocalVoiceUsers.Find(LocalUserNum);
}