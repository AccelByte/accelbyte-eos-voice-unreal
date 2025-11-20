# AccelByteEOSVoice Module

`AccelByteEOSVoice` bridges AccelByte powered matchmaking/sessions with Epic Online Services (EOS) Voice Chat so that game, team, and party members can seamlessly talk to each other once they are matched. It exposes a `UGameInstanceSubsystem` (`UAccelByteEOSVoiceSubsystem`) plus a `ULocalPlayerSubsystem` helper (`UAccelByteEOSVoiceLocalPlayerSubsystem`) so gameplay code and UI widgets can trigger voice actions without touching the underlying SDKs directly.

## What this module is used for
- Creates and maintains an EOS `IVoiceChatUser` for every logged-in AccelByte local user by reusing the AccelByte OpenID access token.
- Listens to AccelByte lobby free-form notifications (`Topic = "EOS_VOICE"`) so it can auto-join the correct EOS voice channels for the user’s current session.
- Provides a thin wrapper around EOS voice features (join/leave channels, mute state, transmit target, player list, debug console commands) that other modules/UI can call.

## Voice chat features
- **Game voice** – joins the `game-session` EOS channel when the server sends a token; used to let everyone in the current match talk globally.
- **Team voice** – joins `team-session` when the match server supplies a token; used for squad/side-only comms.
- **Party voice** – joins `party-session` while the platform party exists; automatically leaves when the party session is destroyed.
  
Each channel is joined through the same API (`JoinChannel`) but with a different `Type` field inside the lobby notification payload. `UAccelByteEOSVoiceLocalPlayerSubsystem::ETransmitChannel` allows UI to switch the user’s active transmit target between Party/Team/Game.

## How it works
1. **Subsystem initialization**
   - On startup the module grabs the AccelByte `IOnlineSubsystem` and hooks `FOnLoginComplete` for each local user.
   - It also registers for session destruction so it can leave the relevant voice rooms when `NAME_GameSession` or `NAME_PartySession` end.
2. **Ensuring account metadata**
   - After an AccelByte login succeeds it pulls the user profile via `GetUserApi().GetData`. EOS voice requires a display name, so if the AccelByte account does not have one the subsystem auto-generates a temporary display/unique name (if `bAutoFillEmptyDisplayName` is `true`).
3. **Logging in to EOS Voice**
   - Once the account has a display name, `LoginToEpic` uses the AccelByte OpenID access token (`Type = AccelByte:OpenIdAccessToken`) to log the same local user into the EOS Online Subsystem.
   - When EOS login finishes, the subsystem caches the `IVoiceChatUser` interface, unmutes the mic by default, and exposes helper methods for joining, leaving, muting, deafening, and enumerating channels/players.
4. **Receiving join tokens**
   - The AccelByte Lobby message delegate listens for `Topic = EOS_VOICE`. The payload is parsed into `FVoiceChatJoinToken`, which contains the channel type (`game-session`, `party-session`, or `team-session`), participant token, and voice server URL.
   - The subsystem converts that payload into `FEOSVoiceChatChannelCredentials` JSON and calls `IVoiceChatUser::JoinChannel`. All channels are joined as non-positional.
5. **Local player access**
   - Gameplay/UI code should talk to `UAccelByteEOSVoiceLocalPlayerSubsystem`. It forwards requests to the shared game instance subsystem, ensuring the correct `LocalUserNum` is used. This layer also registers for player-added events if you want to show in-channel rosters.
6. **Debug helpers**
   - Development builds include console commands such as `AB.EOSVoice.TransmitTo <channel>`, `AB.EOSVoice.SetMute true|false`, `AB.EOSVoice.SetDeafen true|false`, and `AB.EOSVoice.ListPlayers` to inspect the live voice state.

## Dependencies & configuration
- **Online subsystems**
  - `OnlineSubsystemAccelByte` (provides identity/session + Lobby notifications)
  - `AccelByteUe4Sdk` (User API, Lobby API)
  - `OnlineSubsystemEOS` + `EOSVoiceChat` (actual transport and `IVoiceChatUser`)
  - `VoiceChat` module for the engine-facing interfaces
- **Voice token service**
  - The AccelByte backend must run the [extend-eos-voice-rtc](https://github.com/AccelByte/extend-eos-voice-rtc) app so that game/team/party servers can mint the lobby notifications that contain EOS voice join tokens.
- **Epic login requirements**
  - EOS Voice requires that each user logs into EOS with a valid OAuth/OpenID token. The subsystem obtains this token from the AccelByte identity interface and performs the `IOnlineIdentity::Login` call on the EOS subsystem automatically.
- **Display name requirements**
  - EOS Voice rejects accounts whose display name is empty. Make sure users have `DisplayName` and `UniqueDisplayName` set in AccelByte. When testing with device accounts or guest accounts, enable the auto-fill option below so the subsystem can create placeholder values; if you disable it, ensure your own display-name provisioning completes before calling `UAccelByteEOSVoiceSubsystem::LoginToEpic(LocalUserNum)` to resume the login flow.
- **Configuration**

  ```ini
  ; DefaultEngine.ini
  [/Script/AccelByteEOSVoice.AccelByteEOSVoiceConfig]
  bAutoFillEmptyDisplayName=true   ; allows the subsystem to generate "Player-XXXX" style names when needed
  ```

  Turning this flag off means you must provision display names yourself, otherwise the EOS login step aborts and voice cannot start.
- **Session + lobby services**
  - Voice channel tokens are delivered over the AccelByte Lobby free-form message bus (`EOS_VOICE` topic). Ensure your backend matchmaking/game-session flow publishes these notifications whenever a player should join a party/team/game voice room.

### Engine-side EOS configuration
`DefaultEngine.ini` needs to opt-in to EOS Connect and disable the EAS (Epic Account Services) login flow in favor of the new OpenID flow used by AccelByte tokens:

```ini
[/Script/OnlineSubsystemEOS.EOSSettings]
bUseEAS=false
bUseEOSConnect=true
bUseNewLoginFlow=true
```

### Epic Developer Portal setup
Configure EOS to trust AccelByte as an OpenID identity provider so the EOS Connect login call above can exchange the AccelByte access token:

1. Open your product in the EOS Developer Portal: **Product Settings → Identity Providers → Add Identity Provider**.
2. Fill the form with:
   - Identity Provider: `OpenID`
   - Description: `AccelByte`
   - Type: `UserInfo Endpoint`
   - UserInfo API Endpoint: `https://<studio>.prod.gamingservices.accelbyte.io/iam/v3/public/users/me`
   - HTTP Method: `GET`
   - AccountId: `userId`
   - DisplayName: `displayName`
3. Save the provider and attach it to the deployment/product you use for testing.
- **Player groups for dev/stage sandboxes**
  - When exercising EOS voice in non-production environments, add the AccelByte accounts you plan to test with into an EOS Player Group so Connect/OpenID can trust them. In the Epic Developer Portal, create (or edit) a player group with **Identity Provider = OpenID** and list your AccelByte user IDs under **Account Ids**. Attach that group to the dev/stage sandbox you are using.

With these dependencies satisfied you can access the voice helpers from any local player:

```cpp
auto* Voice = LocalPlayer->GetSubsystem<UAccelByteEOSVoiceLocalPlayerSubsystem>();
Voice->TransmitToSpecificChannel(UAccelByteEOSVoiceLocalPlayerSubsystem::ETransmitChannel::Team);
Voice->SetAudioInputDeviceMuted(false);

// To get the IVoiceChatUser to unlock more functionality, use this function
IVoiceChatUser* VoiceUser = Voice->GetVoiceChatUser();
```

The subsystem takes care of authenticating against both AccelByte and EOS, parsing lobby notifications, and keeping channel membership in sync with the player’s sessions so that Party, Team, and Game chat are always available.
