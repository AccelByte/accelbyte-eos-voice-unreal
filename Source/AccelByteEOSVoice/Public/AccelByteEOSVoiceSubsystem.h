// Copyright (c) 2026 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSessionInterfaceV2AccelByte.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Api/AccelByteEOSVoiceApi.h"
#include "GameServerApi/AccelByteServerEOSVoiceApi.h"
#include "VoiceChat.h"
#include "IOnlineSubsystemEOS.h"
#include "EOSVoiceChatUser.h"
#include "TimerManager.h"
#include "eos_sdk.h"
#include "AccelByteEOSVoiceSubsystem.generated.h"

#define DEFINE_EOS_NOTIFY_STRUCT(StructName, CallbackInfoType, HandlerFn) \
struct F##StructName \
{ \
	EOS_NotificationId Id = EOS_INVALID_NOTIFICATIONID; \
	TWeakObjectPtr<UAccelByteEOSVoiceSubsystem> Owner; \
	static void Trampoline(const CallbackInfoType* Data) \
	{ \
		auto* Self = static_cast<F##StructName*>(Data->ClientData); \
		if (Self && Self->Owner.IsValid()) \
		{ \
			Self->Owner->HandlerFn(*Data); \
		} \
	} \
}; \
F##StructName StructName; \
void HandlerFn(const CallbackInfoType& Data);


UCLASS()
class ACCELBYTEEOSVOICE_API UAccelByteEOSVoiceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void LoginToEpic(int32 LocalUserNum);
    void SetPlayerMuted(const FString& PlayerName, bool bIsMuted);
    void SetAudioInputDeviceMuted(bool bIsMuted);
    void SetAudioOutputDeviceMuted(bool bIsMuted);
    void TransmitToSpecificChannel(EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType ChannelType);

    IVoiceChatUser* GetVoiceChatUser() const { return VoiceChatUser; }
	static FString ToChannelName(EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType ChannelName);

protected:
    bool GetGameSessionId(FName SessionName, FString& OutSessionId) const;
    void HandleAutoJoinVoiceChat(FName SessionName);
    void RequestVoiceToken(EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType ChannelType);
    void JoinVoiceChannel(EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType ChannelName, const FString& RoomId, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType);

    DEFINE_EOS_NOTIFY_STRUCT(EOSPartyVoiceDisconnectNotify, EOS_RTC_DisconnectedCallbackInfo, HandlePartyVoiceDisconnection);
    DEFINE_EOS_NOTIFY_STRUCT(EOSTeamVoiceDisconnectNotify, EOS_RTC_DisconnectedCallbackInfo, HandleTeamVoiceDisconnection);
    DEFINE_EOS_NOTIFY_STRUCT(EOSSessionVoiceDisconnectNotify, EOS_RTC_DisconnectedCallbackInfo, HandleSessionVoiceDisconnection);

private:
    void OnAccelByteLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
    void OnAccelByteGetUserData(const FAccountUserData& Response, int32 LocalUserNum);
    void OnAccelByteUpdateDisplayNameCompleted(const FAccountUserData& Response, int32 LocalUserNum);
    void OnEOSLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
    void OnAccelByteCreateSessionCompleted(FName SessionName, bool bWasSuccessful);
    void OnAccelByteJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnAccelByteDestroySessionCompleted(FName SessionName, bool bWasSuccessful);
    void OnServerReceivedSession(FName SessionName);
	void OnVoiceTokenReceivedFromLobbyNotification(FAccelByteModelsNotificationMessage const& Message);

	void OnSessionVoiceTokenGenerated(const FAccelByteEOSVoiceVoiceSessionTokenResponse& Response);
    void OnVoiceTokenGenerated(const FAccelByteEOSVoiceVoiceEOSTokenResponse& Response);
    void OnVoiceTokenGenerationFailedForChannel(int32 ErrCode, const FString& ErrMsg, EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType ChannelType);

    TMap<EAccelByteEOSVoiceVoiceEOSTokenResponseChannelType, FString> RoomIdMap{};

    FOnlineIdentityAccelBytePtr IdentityAccelByte;
    FOnlineSessionV2AccelBytePtr SessionAccelByte;
    IOnlineSubsystemEOS* EOSSubsystem;
    IOnlineIdentityPtr IdentityEOS;
    FEOSVoiceChatUser* VoiceChatUser = nullptr;
    TSharedPtr<AccelByte::Api::EOSVoice> EOSVoiceApi;
    TSharedPtr<AccelByte::GameServerApi::EOSVoice> ServerEOSVoiceApi;
    FDelegateHandle ChannelExitedHandle;
    FString EpicPUID{};
    bool bIsShuttingDown{ false };
    EOS_HRTC EOSRtcHandle = nullptr;
};

UCLASS(Config = Engine, DefaultConfig)
class ACCELBYTEEOSVOICE_API UAccelByteEOSVoiceConfig : public UObject 
{
    GENERATED_BODY()

public:
    /** On Party Creation or Joined, user will automatically request party voice token and join to the room */
    UPROPERTY(Config, EditAnywhere)
    bool bAutoJoinPartyVoice{ true };
    /** On Session Joined, user will automatically request team voice token and join to the room */
    UPROPERTY(Config, EditAnywhere)
    bool bAutoJoinTeamVoice{ false };
    /** On Session Joined, user will automatically request session-wide voice token and join to the room */
    UPROPERTY(Config, EditAnywhere)
    bool bAutoJoinSessionVoice{ false };
    /** On DS received a session, DS will automatically request team voice token to all players in the session.
     * User will receive a lobby notification with voice token information */
    UPROPERTY(Config, EditAnywhere)
    bool bServerAutoGenerateTeamVoiceToken { false };
    /** On DS received a session, DS will automatically request session-wide voice token to all players in the session
     * User will receive a lobby notification with voice token information */
    UPROPERTY(Config, EditAnywhere)
    bool bServerAutoGenerateSessionVoiceToken{ false };
    UPROPERTY(Config, EditAnywhere)
    bool bAutoGenerateDisplayNameIfEmpty{ false };
};

#undef DEFINE_EOS_NOTIFY_STRUCT