// Copyright (c) 2026 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "AccelByteEOSVoiceSubsystem.h"
#include "AccelByteEOSVoice.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemAccelByteDefines.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "OnlineSessionSettings.h"
#include "EOSVoiceChatTypes.h"
#include "TimerManager.h"
#include "IEOSSDKManager.h"
#include "eos_rtc.h"

#define EOS_VOICE_TOPIC TEXT("EOS_VOICE")

void UAccelByteEOSVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FOnlineSubsystemAccelByte* AccelByteSubsystem = static_cast<FOnlineSubsystemAccelByte*>(Online::GetSubsystem(GetWorld(), ACCELBYTE_SUBSYSTEM));
    check(AccelByteSubsystem);

    IdentityAccelByte = StaticCastSharedPtr<FOnlineIdentityAccelByte>(AccelByteSubsystem->GetIdentityInterface());
    check(IdentityAccelByte);

    SessionAccelByte = StaticCastSharedPtr<FOnlineSessionV2AccelByte>(AccelByteSubsystem->GetSessionInterface());
    check(SessionAccelByte);

    EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld(), EOS_SUBSYSTEM));
    check(EOSSubsystem);

    IEOSPlatformHandlePtr PlatformHandle = EOSSubsystem->GetEOSPlatformHandle();
    EOSRtcHandle = EOS_Platform_GetRTCInterface(*PlatformHandle);

    IdentityEOS = EOSSubsystem->GetIdentityInterface();
    check(IdentityEOS);

    if (!IsRunningDedicatedServer())
    {
        IdentityAccelByte->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteLoginCompleted));
        SessionAccelByte->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteCreateSessionCompleted));
        SessionAccelByte->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteJoinSessionCompleted));
        SessionAccelByte->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteDestroySessionCompleted));

        IdentityEOS->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnEOSLoginCompleted));
    }
    else
    {
        ServerEOSVoiceApi = AccelByteSubsystem->GetAccelByteInstance().Pin()->GetServerApiClient()->GetServerApiPtr<AccelByte::GameServerApi::EOSVoice>();
        SessionAccelByte->AddOnServerReceivedSessionDelegate_Handle(FOnServerReceivedSessionDelegate::CreateUObject(this,  &UAccelByteEOSVoiceSubsystem::OnServerReceivedSession));
    }
}

void UAccelByteEOSVoiceSubsystem::Deinitialize()
{
    bIsShuttingDown = true;

    if (VoiceChatUser != nullptr) 
    {
        const TArray<FString> Channels = VoiceChatUser->GetChannels();
        for (const FString& ChannelName : Channels)
        {
            ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Leave channel %s"), *ChannelName);
            VoiceChatUser->LeaveChannel(ChannelName, {});
        }
        VoiceChatUser = nullptr;
    }

    Super::Deinitialize();
}

void UAccelByteEOSVoiceSubsystem::SetPlayerMuted(const FString& PlayerName, bool bIsMuted)
{
    VoiceChatUser->SetPlayerMuted(PlayerName, bIsMuted);
}

void UAccelByteEOSVoiceSubsystem::SetAudioInputDeviceMuted(bool bIsMuted)
{
    VoiceChatUser->SetAudioInputDeviceMuted(bIsMuted);
}

void UAccelByteEOSVoiceSubsystem::SetAudioOutputDeviceMuted(bool bIsMuted)
{
    VoiceChatUser->SetAudioOutputDeviceMuted(bIsMuted);
}

void UAccelByteEOSVoiceSubsystem::TransmitToSpecificChannel(EAccelByteEOSVoiceVoiceChannelType ChannelType)
{
    FString ChannelName = ToChannelName(ChannelType);
    if (ChannelName.IsEmpty())
    {
        VoiceChatUser->TransmitToNoChannels();
    }
    else
    {
        VoiceChatUser->TransmitToSpecificChannels({ ChannelName });
    }
}

void UAccelByteEOSVoiceSubsystem::HandlePartyVoiceDisconnection(const EOS_RTC_DisconnectedCallbackInfo& Data)
{
    // make sure retryable
    bool bShouldReconnect = Data.ResultCode == EOS_EResult::EOS_NoConnection ||
        Data.ResultCode == EOS_EResult::EOS_ServiceFailure ||
        Data.ResultCode == EOS_EResult::EOS_UnexpectedError;
    if (!bShouldReconnect)
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Voice Chat disconnected, no need to reconnect"));
        return;
    }

    FString RoomName = StringCast<TCHAR>(Data.RoomName).Get();
    FString ExpectedRoomName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::PARTY);
    if (RoomName.Equals(ExpectedRoomName))
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Failed to reconnect! RoomName check failed! %s != %s"), *RoomName, *ExpectedRoomName);
        return;
    }

    // TODO: Add proper retry, this only one shot retry.
    RequestVoiceToken(EAccelByteEOSVoiceVoiceChannelType::PARTY);
}

void UAccelByteEOSVoiceSubsystem::HandleTeamVoiceDisconnection(const EOS_RTC_DisconnectedCallbackInfo& Data)
{
    // make sure retryable
    bool bShouldReconnect = Data.ResultCode == EOS_EResult::EOS_NoConnection ||
        Data.ResultCode == EOS_EResult::EOS_ServiceFailure ||
        Data.ResultCode == EOS_EResult::EOS_UnexpectedError;
    if (!bShouldReconnect)
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Voice Chat disconnected, no need to reconnect"));
        return;
    }

    FString RoomName = StringCast<TCHAR>(Data.RoomName).Get();
    FString ExpectedRoomName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::TEAM);
    if (RoomName.Equals(ExpectedRoomName))
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Failed to reconnect! RoomName check failed! %s != %s"), *RoomName, *ExpectedRoomName);
        return;
    }
    RequestVoiceToken(EAccelByteEOSVoiceVoiceChannelType::TEAM);
}

void UAccelByteEOSVoiceSubsystem::HandleSessionVoiceDisconnection(const EOS_RTC_DisconnectedCallbackInfo& Data)
{
    // make sure retryable
    bool bShouldReconnect = Data.ResultCode == EOS_EResult::EOS_NoConnection ||
        Data.ResultCode == EOS_EResult::EOS_ServiceFailure ||
        Data.ResultCode == EOS_EResult::EOS_UnexpectedError;
    if (!bShouldReconnect)
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Voice Chat disconnected, no need to reconnect"));
        return;
    }

    FString RoomName = StringCast<TCHAR>(Data.RoomName).Get();
    FString ExpectedRoomName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::SESSION);
    if (RoomName.Equals(ExpectedRoomName))
    {
        ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Failed to reconnect! RoomName check failed! %s != %s"), *RoomName, *ExpectedRoomName);
        return;
    }
    RequestVoiceToken(EAccelByteEOSVoiceVoiceChannelType::SESSION);
}

void UAccelByteEOSVoiceSubsystem::LoginToEpic(int32 LocalUserNum)
{
    ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Start login to EOS for LocalUserNum %d"), LocalUserNum);

    FOnlineAccountCredentials EpicCreds;
    EpicCreds.Type = TEXT("AccelByte:OpenIdAccessToken");
    EpicCreds.Token = IdentityAccelByte->GetAuthToken(LocalUserNum);

    IdentityEOS->Login(LocalUserNum, EpicCreds);
}

FString UAccelByteEOSVoiceSubsystem::ToChannelName(EAccelByteEOSVoiceVoiceChannelType ChannelName)
{
    switch(ChannelName)
    {
    case EAccelByteEOSVoiceVoiceChannelType::TEAM:
        return FString(TEXT("TEAM"));
    case EAccelByteEOSVoiceVoiceChannelType::PARTY:
        return FString(TEXT("PARTY"));
    case EAccelByteEOSVoiceVoiceChannelType::SESSION:
        return FString(TEXT("SESSION"));
    default:
        return FString(TEXT("INVALID"));
    }
}

bool UAccelByteEOSVoiceSubsystem::GetGameSessionId(FName SessionName, FString& OutSessionId) const
{
    FNamedOnlineSession* NamedSession = SessionAccelByte->GetNamedSession(SessionName);
    if (NamedSession == nullptr)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Session [%s] not found!"), *SessionName.ToString());
        return false;
    }

    OutSessionId = NamedSession->GetSessionIdStr();
    return true;
}

void UAccelByteEOSVoiceSubsystem::HandleAutoJoinVoiceChat(FName SessionName)
{
    const UAccelByteEOSVoiceConfig* VoiceConfig = GetDefault<UAccelByteEOSVoiceConfig>();
    check(VoiceConfig);

    FString SessionId;

    if (GetGameSessionId(SessionName, SessionId))
    {
        if (SessionName.IsEqual(NAME_GameSession))
        {
            FAccelByteEOSVoiceVoiceGenerateSessionTokenBody Request;
            Request.HardMuted = false;
            Request.Puid = EpicPUID;
            Request.Session = VoiceConfig->bAutoJoinSessionVoice;
            Request.Team = VoiceConfig->bAutoJoinTeamVoice;
            if (Request.Session || Request.Team)
            {
                EOSVoiceApi->VoiceGenerateSessionToken(SessionId, Request,
                    AccelByte::THandler<FAccelByteEOSVoiceVoiceSessionTokenResponse>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnSessionVoiceTokenGenerated),
                    FErrorHandler::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerationFailedForChannel, EAccelByteEOSVoiceVoiceChannelType::SESSION));
            }
        }
        else if (SessionName.IsEqual(NAME_PartySession))
        {
            if (VoiceConfig->bAutoJoinPartyVoice)
            {
                FAccelByteEOSVoiceVoiceGeneratePartyTokenBody Request;
                Request.HardMuted = false;
                Request.Puid = EpicPUID;
                EOSVoiceApi->VoiceGeneratePartyToken(SessionId, Request,
                    AccelByte::THandler<FAccelByteEOSVoiceVoiceEOSTokenResponse>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerated),
                    FErrorHandler::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerationFailedForChannel, EAccelByteEOSVoiceVoiceChannelType::PARTY));
            }
        }
    }
}

void UAccelByteEOSVoiceSubsystem::RequestVoiceToken(EAccelByteEOSVoiceVoiceChannelType ChannelType)
{
    FString SessionId;
    if(ChannelType == EAccelByteEOSVoiceVoiceChannelType::PARTY)
    {
        GetGameSessionId(NAME_PartySession, SessionId);
        FAccelByteEOSVoiceVoiceGeneratePartyTokenBody Request;
        Request.HardMuted = false;
        Request.Puid = EpicPUID;
        EOSVoiceApi->VoiceGeneratePartyToken(SessionId, Request,
            AccelByte::THandler<FAccelByteEOSVoiceVoiceEOSTokenResponse>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerated),
            FErrorHandler::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerationFailedForChannel, EAccelByteEOSVoiceVoiceChannelType::PARTY));
    }
    else
    {
        GetGameSessionId(NAME_GameSession, SessionId);
        FAccelByteEOSVoiceVoiceGenerateSessionTokenBody Request {
            false, EpicPUID, ChannelType == EAccelByteEOSVoiceVoiceChannelType::SESSION, ChannelType == EAccelByteEOSVoiceVoiceChannelType::TEAM
        };
        EOSVoiceApi->VoiceGenerateSessionToken(SessionId, Request,
            AccelByte::THandler<FAccelByteEOSVoiceVoiceSessionTokenResponse>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnSessionVoiceTokenGenerated),
            FErrorHandler::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerationFailedForChannel, EAccelByteEOSVoiceVoiceChannelType::SESSION));
    }
}

void UAccelByteEOSVoiceSubsystem::JoinVoiceChannel(EAccelByteEOSVoiceVoiceChannelType ChannelName, const FString& RoomId, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType)
{
    if (VoiceChatUser == nullptr)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("VoiceChatUser is already invalid (?). Abort to join voice channel"));
        return;
    }

    // TODO: Voice Join Channel Complete Delegate
    VoiceChatUser->JoinChannel(ToChannelName(ChannelName), ChannelCredentials, ChannelType, {});
}


void UAccelByteEOSVoiceSubsystem::OnAccelByteLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    if (!bWasSuccessful) 
    {
        ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Failed to login to AccelByte, abort login to Epic. LocalUserNum: %d"), LocalUserNum);
        return;
    }

    AccelByte::FApiClientPtr ApiClient = IdentityAccelByte->GetApiClient(LocalUserNum);
    check(ApiClient.IsValid());
    EOSVoiceApi = ApiClient->GetApiPtr<AccelByte::Api::EOSVoice>();
    check(EOSVoiceApi.IsValid());

    ApiClient->GetUserApi().Pin()->GetData(AccelByte::THandler<FAccountUserData>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteGetUserData, LocalUserNum),
        FErrorHandler::CreateWeakLambda(this, [LocalUserNum](int32 ErrCode, const FString& ErrMsg)
            {
                ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Unable to gather user data for LocalUserNum %d! [%d] %s"), LocalUserNum, ErrCode, *ErrMsg);
            })
    );
    ApiClient->GetLobbyApi().Pin()->AddMessageNotifDelegate(AccelByte::Api::Lobby::FMessageNotif::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnVoiceTokenReceivedFromLobbyNotification));

    LoginToEpic(LocalUserNum);
}

void UAccelByteEOSVoiceSubsystem::OnAccelByteGetUserData(const FAccountUserData& Response, int32 LocalUserNum)
{
    const bool bIsDisplayNameValid = !Response.DisplayName.IsEmpty();
    if (!Response.DisplayName.IsEmpty())
    {
        // Directly login to Epic
        LoginToEpic(LocalUserNum);
        return;
    }

    const UAccelByteEOSVoiceConfig* VoiceConfig = GetDefault<UAccelByteEOSVoiceConfig>();
    check(VoiceConfig);
    if (!VoiceConfig->bAutoGenerateDisplayNameIfEmpty)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("bAutoGenerateDisplayNameIfEmpty is set to false. Abort to populate the Display Name! You must handle the DisplayName update manually, after that, you can call the LoginToEpic"));
        return;
    }

    // In EOS Open ID, Display Name must be filled. This will auto generate Display Name with format Player_UID
    AccelByte::FApiClientPtr ApiClient = IdentityAccelByte->GetApiClient(LocalUserNum);
    check(ApiClient.IsValid());

    FUserUpdateRequest UpdateRequest;
    UpdateRequest.DisplayName = FString::Printf(TEXT("Player-%s"), *Response.UserId.Left(4));
    UpdateRequest.UniqueDisplayName = FString::Printf(TEXT("%s-%04d"), *Response.UserId.Left(7), FMath::RandRange(0, 9999));
    ApiClient->GetUserApi().Pin()->UpdateUser(
        UpdateRequest,
        THandler<FAccountUserData>::CreateUObject(this, &UAccelByteEOSVoiceSubsystem::OnAccelByteUpdateDisplayNameCompleted, LocalUserNum),
        FErrorHandler::CreateWeakLambda(this, [LocalUserNum](int32 ErrCode, const FString& ErrMsg)
            {
                ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Unable to update user data for LocalUserNum %d! [%d] %s"), LocalUserNum, ErrCode, *ErrMsg);
            })
    );
}

void UAccelByteEOSVoiceSubsystem::OnAccelByteUpdateDisplayNameCompleted(const FAccountUserData& Response, int32 LocalUserNum)
{
    LoginToEpic(LocalUserNum);
}

void UAccelByteEOSVoiceSubsystem::OnEOSLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error) 
{
    if (!bWasSuccessful)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("Failed to login to EOS voice for LocalUserNum %d: %s"), LocalUserNum, *Error);
        return;
    }
    
    FString EOSUserId = UserId.ToString();
    EOSUserId.Split(TEXT("|"), nullptr, &EpicPUID);

    VoiceChatUser = static_cast<FEOSVoiceChatUser*>(EOSSubsystem->GetVoiceChatUserInterface(UserId));
    // automatically open the voice input for testing purpose
    VoiceChatUser->SetAudioInputDeviceMuted(false);

    ACCELBYTE_EOS_VOICE_LOG(Log, TEXT("Successfully logged in to Epic. LocalUserNum %d. UserId: %s. PUID: %s"), LocalUserNum, *EOSUserId, *EpicPUID);
}

void UAccelByteEOSVoiceSubsystem::UAccelByteEOSVoiceSubsystem::OnAccelByteCreateSessionCompleted(FName SessionName, bool bWasSuccessful) 
{
    if (VoiceChatUser == nullptr) 
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("VoiceChatUser is invalid (?). Abort to join voice channel"));
        return;
    }

    HandleAutoJoinVoiceChat(SessionName);
}

void UAccelByteEOSVoiceSubsystem::OnAccelByteJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result) 
{
    if (VoiceChatUser == nullptr)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("VoiceChatUser is invalid (?). Abort to join voice channel"));
        return;
    }

    HandleAutoJoinVoiceChat(SessionName);
}

void UAccelByteEOSVoiceSubsystem::OnAccelByteDestroySessionCompleted(FName SessionName, bool bWasSuccessful) 
{
    if (VoiceChatUser == nullptr)
    {
        ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("VoiceChatUser is invalid (?). No need to leave voice chat"));
        return;
    }

    if (SessionName.IsEqual(NAME_PartySession) && RoomIdMap.Contains(EAccelByteEOSVoiceVoiceChannelType::PARTY))
    {
        const FString ChannelName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::PARTY);
        RoomIdMap.Remove(EAccelByteEOSVoiceVoiceChannelType::PARTY);
        VoiceChatUser->LeaveChannel(ChannelName, {});
    }
    else if (SessionName.IsEqual(NAME_GameSession))
    {
        if (RoomIdMap.Contains(EAccelByteEOSVoiceVoiceChannelType::TEAM))
        {
            const FString ChannelName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::TEAM);
            RoomIdMap.Remove(EAccelByteEOSVoiceVoiceChannelType::TEAM);
            VoiceChatUser->LeaveChannel(ChannelName, {});
        }
        if (RoomIdMap.Contains(EAccelByteEOSVoiceVoiceChannelType::SESSION))
        {
            const FString ChannelName = ToChannelName(EAccelByteEOSVoiceVoiceChannelType::SESSION);
            RoomIdMap.Remove(EAccelByteEOSVoiceVoiceChannelType::SESSION);
            VoiceChatUser->LeaveChannel(ChannelName, {});
        }
    }
}

void UAccelByteEOSVoiceSubsystem::OnServerReceivedSession(FName SessionName)
{
    const UAccelByteEOSVoiceConfig* VoiceConfig = GetDefault<UAccelByteEOSVoiceConfig>();
    check(VoiceConfig);

    FString SessionId;

    if (GetGameSessionId(SessionName, SessionId)) 
    {
        FAccelByteEOSVoiceVoiceGenerateAdminSessionTokenBody Request;
        Request.HardMuted = false;
        Request.Session = VoiceConfig->bServerAutoGenerateSessionVoiceToken;
        Request.Team = VoiceConfig->bServerAutoGenerateTeamVoiceToken;
        Request.AllowPendingUsers = true;
        Request.Notify = true;
        if(Request.Session || Request.Team)
        {
            ServerEOSVoiceApi->VoiceGenerateAdminSessionToken(SessionId, Request, {}, {});
        }
    }
}

void UAccelByteEOSVoiceSubsystem::OnVoiceTokenReceivedFromLobbyNotification(FAccelByteModelsNotificationMessage const& Message)
{
    if(!Message.Topic.Equals(EOS_VOICE_TOPIC))
    {
        return;
    }
    
    FAccelByteEOSVoiceVoiceSessionTokenResponse JoinToken;
    FJsonObjectConverter::JsonObjectStringToUStruct(Message.Payload, &JoinToken);

    OnSessionVoiceTokenGenerated(JoinToken);
}

void UAccelByteEOSVoiceSubsystem::OnSessionVoiceTokenGenerated(const FAccelByteEOSVoiceVoiceSessionTokenResponse& Response)
{
    for(const FAccelByteEOSVoiceVoiceEOSTokenResponse& VoiceToken : Response.Tokens)
    {
        OnVoiceTokenGenerated(VoiceToken);
    }
}

void UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerated(const FAccelByteEOSVoiceVoiceEOSTokenResponse& Response)
{
    FEOSVoiceChatChannelCredentials Credentials;
    Credentials.ClientBaseUrl = Response.ClientBaseUrl;
    Credentials.ParticipantToken = Response.Token;
    
    const FTCHARToUTF8 Utf8RoomName(*ToChannelName(Response.ChannelType));
    const FTCHARToUTF8 ProductIdUtf8(*EpicPUID);

    EOS_RTC_AddNotifyDisconnectedOptions DisconnectedOptions = {};
    DisconnectedOptions.ApiVersion = EOS_RTC_ADDNOTIFYDISCONNECTED_API_LATEST;
    DisconnectedOptions.RoomName = Utf8RoomName.Get();
    DisconnectedOptions.LocalUserId = EOS_ProductUserId_FromString(ProductIdUtf8.Get());

    if (Response.ChannelType == EAccelByteEOSVoiceVoiceChannelType::PARTY)
    {
        RoomIdMap.Emplace(EAccelByteEOSVoiceVoiceChannelType::PARTY, Response.RoomId);

        if (EOSPartyVoiceDisconnectNotify.Id == 0)
        {
            // Register disconnect event
            EOSPartyVoiceDisconnectNotify.Id = EOS_RTC_AddNotifyDisconnected(EOSRtcHandle, &DisconnectedOptions, &EOSPartyVoiceDisconnectNotify, &UAccelByteEOSVoiceSubsystem::FEOSPartyVoiceDisconnectNotify::Trampoline);
            if (EOSPartyVoiceDisconnectNotify.Id == EOS_INVALID_NOTIFICATIONID)
            {
                ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("BindChannelCallbacks EOS_RTC_AddNotifyDisconnected failed Room Name: %s"), *ToChannelName(Response.ChannelType));
            }
        }
    }
    else if (Response.ChannelType == EAccelByteEOSVoiceVoiceChannelType::TEAM)
    {
        RoomIdMap.Emplace(EAccelByteEOSVoiceVoiceChannelType::TEAM, Response.RoomId);

        if (EOSTeamVoiceDisconnectNotify.Id == 0)
        {
            // Register disconnect event
            EOSTeamVoiceDisconnectNotify.Id = EOS_RTC_AddNotifyDisconnected(EOSRtcHandle, &DisconnectedOptions, &EOSTeamVoiceDisconnectNotify, &UAccelByteEOSVoiceSubsystem::FEOSTeamVoiceDisconnectNotify::Trampoline);
            if (EOSTeamVoiceDisconnectNotify.Id == EOS_INVALID_NOTIFICATIONID)
            {
                ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("BindChannelCallbacks EOS_RTC_AddNotifyDisconnected failed Room Name: %s"), *ToChannelName(Response.ChannelType));
            }
        }
    }
    else if (Response.ChannelType == EAccelByteEOSVoiceVoiceChannelType::SESSION)
    {
        RoomIdMap.Emplace(EAccelByteEOSVoiceVoiceChannelType::SESSION, Response.RoomId);

        if (EOSSessionVoiceDisconnectNotify.Id == 0)
        {
            // Register disconnect event
            EOSSessionVoiceDisconnectNotify.Id = EOS_RTC_AddNotifyDisconnected(EOSRtcHandle, &DisconnectedOptions, &EOSSessionVoiceDisconnectNotify, &UAccelByteEOSVoiceSubsystem::FEOSSessionVoiceDisconnectNotify::Trampoline);
            if (EOSSessionVoiceDisconnectNotify.Id == EOS_INVALID_NOTIFICATIONID)
            {
                ACCELBYTE_EOS_VOICE_LOG(Warning, TEXT("BindChannelCallbacks EOS_RTC_AddNotifyDisconnected failed Room Name: %s"), *ToChannelName(Response.ChannelType));
            }
        }
    }

    JoinVoiceChannel(Response.ChannelType, Response.RoomId, Credentials.ToJson(), EVoiceChatChannelType::NonPositional);
}

void UAccelByteEOSVoiceSubsystem::OnVoiceTokenGenerationFailedForChannel(int32 ErrCode, const FString& ErrMsg, EAccelByteEOSVoiceVoiceChannelType ChannelType)
{
    ACCELBYTE_EOS_VOICE_LOG(Error, TEXT("Failed to generate voice token for channel type %d. [%d] %s"),
        static_cast<int32>(ChannelType), ErrCode, *ErrMsg);
}

#undef EOS_VOICE_TOPIC
