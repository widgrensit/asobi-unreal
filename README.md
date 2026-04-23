# asobi-unreal

Unreal Engine 5 C++ plugin SDK for the [Asobi](https://github.com/widgrensit/asobi) game backend. Requires UE 5.7+.

## Installation

Clone into your project's `Plugins/` directory:

```bash
cd YourProject/Plugins
git clone https://github.com/widgrensit/asobi-unreal.git AsobiSDK
```

Regenerate project files, then enable `Asobi SDK` in *Edit → Plugins → Networking*.

## Quick Start

```cpp
#include "AsobiClient.h"
#include "AsobiAuth.h"
#include "AsobiMatchmaker.h"
#include "AsobiWebSocket.h"

UAsobiClient* Client = NewObject<UAsobiClient>();
Client->SetBaseUrl(TEXT("http://localhost:8080"));

UAsobiAuth* Auth = NewObject<UAsobiAuth>();
Auth->Init(Client);

FOnAsobiAuthResponse OnLogin;
OnLogin.BindDynamic(this, &AMyPawn::HandleLogin);
Auth->Login(TEXT("player1"), TEXT("secret"), OnLogin);
```

## Features

| Subsystem | REST | WebSocket |
|---|---|---|
| Auth (register, login, refresh, OAuth, IAP) | ✓ | — |
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

## License

Apache 2.0
