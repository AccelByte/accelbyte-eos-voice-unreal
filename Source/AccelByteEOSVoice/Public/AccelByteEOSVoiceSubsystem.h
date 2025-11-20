// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "VoiceChat.h"
#include "AccelByteEOSVoiceSubsystem.generated.h"

#define EOS_VOICE_TOKEN_TOPIC TEXT("EOS_VOICE")
#define EOS_VOICE_TEAM_SESSION_CHANNEL TEXT("team-session")
#define EOS_VOICE_PARTY_SESSION_CHANNEL TEXT("party-session")
#define EOS_VOICE_GAME_SESSION_CHANNEL TEXT("game-session")

USTRUCT()
struct FVoiceChatJoinToken {
	GENERATED_BODY()
	
	UPROPERTY()
	FString ClientBaseUrl;

	UPROPERTY()
	FString RoomId;
	
	UPROPERTY()
	FString SessionId;
	
	UPROPERTY()
	FString TeamId;
	
	UPROPERTY()
	FString Token;
	
	UPROPERTY()
	FString Type;
};

/**
 * 
 */
UCLASS()
class ACCELBYTEEOSVOICE_API UAccelByteEOSVoiceSubsystem : public UGameInstanceSubsystem {
	GENERATED_BODY()

public:
	virtual void    Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void	Deinitialize() override;
	bool            JoinChannel(int32 LocalUserNum, const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate = FOnVoiceChatChannelJoinCompleteDelegate());
	bool            LeaveChannel(int32 LocalUserNum, const FString& ChannelName, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate = FOnVoiceChatChannelLeaveCompleteDelegate());
	bool            SetPlayerMuted(int32 LocalUserNum, const FString& PlayerName, bool bIsMuted);
	bool            SetAudioInputDeviceMuted(int32 LocalUserNum, bool bIsMuted);
	bool            SetAudioOutputDeviceMuted(int32 LocalUserNum, bool bIsMuted);
	bool            TransmitToSpecificChannel(int32 LocalUserNum, const FString& ChannelName);
	IVoiceChatUser* GetVoiceChatUser(int32 LocalUserNum) const;
	void            LoginToEpic(int32 LocalUserNum);
	
private:
	void OnAccelByteLoginCompleted(int LocalUserNum, bool bSuccessful, const FUniqueNetId& UniqueNetId, const FString& Error);
	void OnEpicLoginCompleted(int LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueNetId, const FString& Error);
	void OnGetUserData(const FAccountUserData& AccountUserData, int32 LocalUserNum);
	void OnUpdateDisplayNameCompleted(const FAccountUserData& AccountUserData, int32 LocalUserNum);
	void OnVoiceTokenReceived(FAccelByteModelsNotificationMessage const& AccelByteModelsNotificationMessage, int32 LocalUserNum);
	void OnSessionDestroyed(FName SessionName, bool bWasSuccess);

	struct FAccelByteVoiceLocalUser
	{
		AccelByte::FApiClientPtr ApiClient;
		FDelegateHandle LobbyMessageDelegateHandle;
		IVoiceChatUser* VoiceChatUser = nullptr;
	};

	FAccelByteVoiceLocalUser* FindVoiceLocalUser(int32 LocalUserNum);
	const FAccelByteVoiceLocalUser* FindVoiceLocalUser(int32 LocalUserNum) const;

	FOnlineIdentityAccelBytePtr IdentityAccelByte;
	IOnlineIdentityPtr IdentityEOS;
	TMap<int32, FAccelByteVoiceLocalUser> LocalVoiceUsers;
};


UCLASS(Config = Engine, DefaultConfig)
class ACCELBYTEEOSVOICE_API UAccelByteEOSVoiceConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere)
	bool bAutoFillEmptyDisplayName { true };
};