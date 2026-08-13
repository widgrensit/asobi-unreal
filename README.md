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
	FOnAsobiAuthResponse::CreateLambda([](bool bOk, const FAsobiAuthTokens& Tokens, const FAsobiError& Error)
	{
		// Tokens are stored on the client automatically, same as Login/OAuth.
		// Tokens.bCreated distinguishes a new guest (true) from a resumed one (false);
		// Tokens.bGuest is true and Tokens.Username carries the assigned guest name.
		if (!bOk)
		{
			// Error.StatusCode / Error.Reason, e.g. 400 weak_device_secret,
			// 409 device_already_registered, 503 guest_capacity_reached.
		}
	}));
```

Later, convert the guest into a permanent account. The call is authenticated with the guest's current access token and replaces the stored token pair with the claimed account's:

```cpp
Auth->UpgradeGuest(TEXT("player1"), TEXT("secret"), OnUpgrade);
```

On success the returned `FAsobiAuthTokens` has `bUpgraded == true`.

### Auth failures

Every auth entrypoint (`Register`, `Login`, `Refresh`, `OAuthAuthenticate`, `Guest`, `GuestDevice`, `UpgradeGuest`) reports failures through the third callback parameter:

```cpp
FOnAsobiAuthResponse::CreateLambda([](bool bOk, const FAsobiAuthTokens& Tokens, const FAsobiError& Error)
{
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("auth failed: %d %s"), Error.StatusCode, *Error.Reason);
	}
});
```

`Error.Reason` is the backend's stable snake_case code -- `weak_device_secret`, `invalid_device_secret`, `guest_revoked`, `guest_capacity_reached`, `device_already_registered`, `not_an_unclaimed_guest`, `username_taken`, `invalid_credentials`, `validation_failed`, ... -- so you can branch on it rather than showing one generic message. Three reasons are produced by the SDK itself: `network_error` (no response, `StatusCode == 0`), `malformed_response` (2xx with no usable access token) and `unknown_error` (a non-2xx body with no `error` field).

#### Guest device (managed credentials)

`GuestDevice` is the one-call version: it generates the `(DeviceId, DeviceSecret)` pair on first run, persists it to a `USaveGame` slot, reuses it on every later launch, and signs in — so you never hand-roll base64, storage, or the >=32-byte rule.

```cpp
Auth->GuestDevice(
	FOnAsobiAuthResponse::CreateLambda([](bool bOk, const FAsobiAuthTokens& Tokens, const FAsobiError& Error)
	{
		if (!bOk) return;
		// Tokens.bCreated == true on the very first sign-in (brand-new guest),
		// false when the persisted pair resumed an existing player.
	}));
```

To forget the guest ("switch account" / "play as someone else"), erase the stored pair — the next `GuestDevice` mints a brand-new guest. This is local-only; pair it with `Logout` to end the current session, or call `UpgradeGuest` first to keep the player:

```cpp
#include "AsobiDevice.h"

AsobiDevice::Clear();
```

### Deleting the account

`AsobiDevice::Clear` deletes nothing on the server — the account and its data stay, merely unreachable from that install. For an actual "delete my data" request, and for the in-app account deletion the app stores require, erase it:

```cpp
// Guest or provider-only account: no password to confirm with.
Client->Auth->EraseAccount(TEXT(""), FOnAsobiResponse::CreateLambda(
	[](bool bSuccess, const FString& Response) { /* ... */ }));

// Account with a password: it must be echoed.
Client->Auth->EraseAccount(TEXT("secret123"), Callback);
```

Irreversible. A wrong password comes back `403` with code `player.confirmation_failed` and changes nothing. On success the local session is cleared, because the server deleted the token pair in the same transaction; anything afterwards on that session is a `401`, which for a retried erase means it already worked.

Needs a server carrying `POST /api/v1/players/me/erase`; older ones answer 404.

The default byte source is **best-effort**, not a guaranteed CSPRNG on every platform (UE ships no portable one). For higher assurance, or to choose your own storage slot, drive `AsobiDevice::LoadOrCreate` with options and pass the pair to `Guest()`:

```cpp
FAsobiDeviceOptions Options;
Options.SlotName = TEXT("MyGameGuest");
Options.RandomBytes = [](int32 NumBytes) { return MyPlatformCsprng(NumBytes); };

const FAsobiDeviceCredentials Creds = AsobiDevice::LoadOrCreate(Options);
Auth->Guest(Creds.DeviceId, Creds.DeviceSecret, OnGuest);
```

## Client-side prediction

`WorldInput` sends no sequence number, and the server acks only connections that
stamped one. To reconcile a prediction, stamp every input with your own counter
via `WorldInputWithSeq` and bind `OnWorldAck`:

```cpp
WebSocket->OnWorldAck.AddDynamic(this, &UMyClass::HandleWorldAck);

// Seq is yours to generate and increment; the SDK does neither.
// NextSeq is an int64 member of your class.
WebSocket->WorldInputWithSeq(TEXT("{\"move_x\":1}"), ++NextSeq);
```

```cpp
void UMyClass::HandleWorldAck(const FAsobiWorldAck& Ack)
{
	// Ack.Tick and Ack.Seq are both int64.
}
```

`WorldInputWithSeq(const FString& DataJson, int64 Seq)` is a second Blueprint
node rather than an extra pin on `WorldInput(const FString& DataJson)`, because
UFUNCTIONs cannot overload by name. Both wrap `DataJson` as `payload.data`; only
`WorldInputWithSeq` stamps `seq`, and it rides as a top-level sibling of
`payload`, never nested:

```json
{"type":"world.input","cid":"c-7","seq":412,"payload":{"data":{"move_x":1}}}
{"type":"world.ack","payload":{"tick":42,"seq":412}}
```

`Ack.Seq` is a high-water mark: the highest input the server had consumed for you
as of `Ack.Tick`, not a receipt for one input. A rejected input still advances
it, so a dropped input never strands the client.

Keep `Seq` within `0 .. 9007199254740991` (2^53-1). The server drops anything
outside that range and sends no ack, silently, so do not seed the counter from a
nanosecond timestamp even though `int64` holds one. The SDK stores no counter of
its own, and the server forgets your recorded seq once you leave the zone or the
socket drops, so restarting the counter after a rejoin is safe.

### Reconciling against world.tick

`OnWorldTick(int64 Tick, const FString& UpdatesJson)` delivers a **delta**, not a
snapshot. `UpdatesJson` is the `updates` array; each entry carries an `op`:

| `op` | Meaning | Fields |
|---|---|---|
| `"a"` | Added, full state | `id` plus every field on the entity |
| `"u"` | Updated, diff | `id` plus only the changed fields |
| `"r"` | Removed | `id` only |

Only the first tick after joining is a full snapshot, so accumulate the deltas
into your own state map. Assigning `UpdatesJson` wholesale to an "authoritative
state" variable is wrong, and a map seeded only from `"u"` entries stays empty,
because your own player first arrives as an `"a"`.

For one tick the server sends `world.tick` first and `world.ack` second, on the
same connection, so prune and replay in the ack handler - while `OnWorldTick`
runs, the pending buffer has not been pruned yet. The loop:

1. Increment `NextSeq`, apply the input locally, and keep it in a `TMap<int64, FMyInput>` of pending inputs.
2. Send it with `WorldInputWithSeq`.
3. Fold every `OnWorldTick` delta into your authoritative state.
4. On `OnWorldAck`, drop every pending input with `Seq <= Ack.Seq`, then re-apply the remainder in `Seq` order on top of that state.

The ack only rides broadcast ticks. `broadcast_interval` defaults to 3
simulation ticks; set it to 1 for an ack every tick, which is what prediction
wants ([world server config](https://asobi.dev/docs/world-server)).

Binding `OnWorldAck` is not enough on its own: keep calling plain `WorldInput`
and nothing ever fires, with no error. Needs plugin v1.4.0 or newer -
`OnWorldAck` and `WorldInputWithSeq` do not exist before it - and a server
carrying `world.ack` (asobi >= v0.84.0); an older server sends no ack rather than
an error.

Full frame reference: [client-side prediction](https://asobi.dev/docs/protocols/websocket#client-side-prediction).

## Extensions (RPC)

Server extensions expose methods over the same socket.

**Blueprint** — drag off the `Asobi RPC` node:

```
Asobi RPC (Realtime, "quests.claim", "{\"quest_key\":\"daily\"}")
  -> Success  ResultJson
  -> Failure  Error.Code / Error.Message / Error.DetailsJson
```

**C++**:

```cpp
TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject);
Params->SetStringField(TEXT("quest_key"), TEXT("daily"));

Realtime->Rpc(TEXT("quests.claim"), Params,
    [](const FString& ResultJson, const FAsobiRpcError* Error)
    {
        if (Error)
        {
            if (Error->Code == TEXT("quests.already_claimed")) { /* ... */ }
            return;
        }
        // ResultJson is the result object - parse it as the extension documents
    });
```

`ResultJson` is raw JSON because the extension defines its shape. Calls are
correlated by cid, so several can be in flight at once and may answer out of
order.

Exactly one outcome fires, always: a call made while disconnected fails with
code `not_connected` rather than leaving a latent node waiting forever. Branch
on `Code`; `Message` is for humans and may be reworded at any time.

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
| Extensions (RPC) | — | ✓ |

Blueprint-callable on every subsystem. Typed `USTRUCT` responses for player, world, match, DM, leaderboard, etc.

## Modules

- `UAsobiClient` — HTTP + JSON helpers, auth token store
- `UAsobiAuth`, `UAsobiMatch`, `UAsobiMatchmaker`, `UAsobiWorld`, `UAsobiEconomy`, `UAsobiLeaderboard`, `UAsobiSocial`, `UAsobiStorage`, `UAsobiTournament`, `UAsobiDirectMessage`
- `UAsobiWebSocket` — real-time client with typed multicast delegates (`OnMatchState`, `OnWorldTick`, `OnWorldAck`, `OnWorldTerrain`, `OnDmMessage`, …)

## Demo

See [asobi-unreal-demo](https://github.com/widgrensit/asobi-unreal-demo) for a working UE5 sample project.

## Smoke test

Every release of this plugin runs the canonical SDK smoke flow against `widgrensit/sdk_demo_backend`. See [`Source/AsobiSDK/Tests/SmokeTest.md`](Source/AsobiSDK/Tests/SmokeTest.md) for the manual run command and CI notes, and [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md) for what CI does and does not cover per release.

## License

Apache 2.0
