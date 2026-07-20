# asobi-unreal

Unreal Engine 5 C++ plugin SDK for the [Asobi](https://github.com/widgrensit/asobi) game backend. Requires UE 5.4+ (tested on 5.4–5.7).

## Installation

Clone into your project's `Plugins/` directory:

```bash
cd YourProject/Plugins
git clone https://github.com/widgrensit/asobi-unreal.git AsobiSDK
```

Regenerate project files, then enable `Asobi SDK` in *Edit → Plugins → Networking*.

## Run a backend first

The SDK talks to an Asobi server. The fastest way to get one is:

```bash
git clone https://github.com/widgrensit/sdk_demo_backend
cd sdk_demo_backend && docker compose up -d
```

That serves at `http://localhost:8084` (HTTP + WebSocket on `/ws`) with a 2-player `demo` mode. For the full reference game (arena shooter) see [`asobi_arena_lua`](https://github.com/widgrensit/asobi_arena_lua).

## Quick Start

```cpp
#include "AsobiClient.h"
#include "AsobiAuth.h"
#include "AsobiMatchmaker.h"
#include "AsobiWebSocket.h"

UAsobiClient* Client = NewObject<UAsobiClient>();
Client->SetBaseUrl(TEXT("http://localhost:8084"));

UAsobiAuth* Auth = NewObject<UAsobiAuth>();
Auth->Init(Client);

FOnAsobiAuthResponse OnLogin;
OnLogin.BindDynamic(this, &AMyPawn::HandleLogin);
Auth->Login(TEXT("player1"), TEXT("secret"), OnLogin);
```

### Listening for a match

The matchmaker pushes `match.matched` after pairing players; a client-initiated `match.join` produces `match.joined`. Subscribe to **both** to cover matchmade and direct-join paths:

```cpp
WebSocket->OnMatchMatched.AddDynamic(this, &UMyClass::OnReady);
WebSocket->OnMatchJoined.AddDynamic(this, &UMyClass::OnReady);
```

See the [WebSocket protocol guide](https://github.com/widgrensit/asobi/blob/main/guides/websocket-protocol.md) for the full event surface.

### Guest / anonymous auth

Sign in without a username by pairing a stable device id with a device secret. The secret must be the base64 of at least 32 CSPRNG bytes, generated and stored by your game (the SDK passes it through, it does not generate or persist it for you). The same `(DeviceId, DeviceSecret)` pair resumes the same guest player on a later launch; store both securely on device.

```cpp
Auth->Guest(DeviceId, DeviceSecret,
	FOnAsobiAuthResponse::CreateLambda([](bool bOk, const FAsobiAuthTokens& Tokens)
	{
		// Tokens are stored on the client automatically, same as Login/OAuth.
		// Tokens.bCreated distinguishes a new guest (true) from a resumed one (false);
		// Tokens.bGuest is true and Tokens.Username carries the assigned guest name.
	}));
```

Later, convert the guest into a permanent account. The call is authenticated with the guest's current access token and replaces the stored token pair with the claimed account's:

```cpp
Auth->UpgradeGuest(TEXT("player1"), TEXT("secret"), OnUpgrade);
```

On success the returned `FAsobiAuthTokens` has `bUpgraded == true`.

## Features

| Subsystem | REST | WebSocket |
|---|---|---|
| Auth (register, login, guest, refresh, OAuth, IAP) | ✓ | — |
| Players & stats | ✓ | — |
| Matches & matchmaker | ✓ | ✓ |
| Worlds (MMO-scale, terrain streaming) | ✓ | ✓ |
| Chat & direct messages | ✓ | ✓ |
| Social (friends, groups) | ✓ | — |
| Economy (wallets, store, inventory) | ✓ | — |
| Leaderboards & tournaments | ✓ | — |
| Cloud saves & storage | ✓ | — |
| Presence & notifications | ✓ | ✓ |
| Voting (cast, veto) | ✓ | ✓ |

Blueprint-callable on every subsystem. Typed `USTRUCT` responses for player, world, match, DM, leaderboard, etc.

## Modules

- `UAsobiClient` — HTTP + JSON helpers, auth token store
- `UAsobiAuth`, `UAsobiMatch`, `UAsobiMatchmaker`, `UAsobiWorld`, `UAsobiEconomy`, `UAsobiLeaderboard`, `UAsobiSocial`, `UAsobiStorage`, `UAsobiTournament`, `UAsobiDirectMessage`
- `UAsobiWebSocket` — real-time client with typed multicast delegates (`OnMatchState`, `OnWorldTick`, `OnWorldTerrain`, `OnDmMessage`, …)

## Demo

See [asobi-unreal-demo](https://github.com/widgrensit/asobi-unreal-demo) for a working UE5 sample project.

## Smoke test

Every release of this plugin runs the canonical SDK smoke flow against `widgrensit/sdk_demo_backend`. See [`Source/AsobiSDK/Tests/SmokeTest.md`](Source/AsobiSDK/Tests/SmokeTest.md) for the manual run command and CI notes.

## License

Apache 2.0
