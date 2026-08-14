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

`WorldInput` sends no sequence number, and the server acks only players who
stamped one. To reconcile a prediction, stamp every input with your own counter
via `WorldInputWithSeq` and bind `OnWorldAck`.

`AddDynamic` binds by name, not by address: the method pointer it takes is only a
type check, and the delegate stores the handler's `FName` and looks it up in the
class's `UFUNCTION` table. Declare the handler `UFUNCTION()`, or the code
compiles and the bind fails at runtime:

```cpp
// MyClass.h
#include "AsobiWebSocket.h"
#include "MyClass.generated.h"

UCLASS()
class UMyClass : public UObject
{
	GENERATED_BODY()

public:
	void Play(UAsobiWebSocket* WebSocket);

	UFUNCTION()
	void HandleWorldAck(const FAsobiWorldAck& Ack);

private:
	int64 NextSeq = 0;
	int64 AckedSeq = -1;
};
```

```cpp
// MyClass.cpp
void UMyClass::Play(UAsobiWebSocket* WebSocket)
{
	WebSocket->OnWorldAck.AddDynamic(this, &UMyClass::HandleWorldAck);

	// Seq is yours to generate and increment; the SDK does neither.
	WebSocket->WorldInputWithSeq(TEXT("{\"move_x\":1}"), ++NextSeq);
}

void UMyClass::HandleWorldAck(const FAsobiWorldAck& Ack)
{
	// Ack.Tick and Ack.Seq are both int64. Acks arrive from every zone you are
	// subscribed to, so Ack.Seq is not monotonic - keep a running maximum.
	if (Ack.Seq <= AckedSeq)
	{
		return;
	}
	AckedSeq = Ack.Seq;
}
```

`WorldInputWithSeq(const FString& DataJson, int64 Seq)` is a second Blueprint
node rather than an extra pin on `WorldInput(const FString& DataJson)`, because
UFUNCTIONs cannot overload by name. Both send `DataJson` as the payload itself,
because that is the map the server forwards to your `handle_input/3`; only
`WorldInputWithSeq` stamps `seq`, and it rides as a top-level sibling of
`payload`, never nested:

```json
{"type":"world.input","cid":"c-7","seq":412,"payload":{"move_x":1}}
{"type":"world.ack","payload":{"tick":42,"seq":412}}
```

`DataJson` must be a JSON object. An empty string sends an empty map; an array,
a bare value or text that is not JSON is dropped with a `[asobi]` log line
rather than going out as the empty map it would otherwise become, which reads
from the game as input that does nothing. Logged once per socket, since a send
loop repeats the same mistake every frame.

`data` is reserved at the top level of that map. The server unwraps it and
drops every sibling key, so `{"action":"fire","data":{"x":1}}` reaches your
game as `{"x":1}` alone. Tracked as
[widgrensit/asobi#478](https://github.com/widgrensit/asobi/issues/478).
`SendMatchInput` is a separate frame with its own `data` handling and is
unaffected.

`Ack.Seq` is a high-water mark: the highest input the sending zone had consumed
for you as of `Ack.Tick`, not a receipt for one input. A rejected input still
advances it, so a dropped input never strands the client. `Ack.Tick` is that
zone's broadcast tick number, the same counter `OnWorldTick` reports on a delta
frame. The snapshot a new zone subscription opens with is not a delta and goes
out as tick `0`, so do not read every `OnWorldTick` tick as a step forward in
time.

Keep `Seq` within `0 .. 9007199254740991` (2^53-1). A value outside that range is
ignored, but the input is not: it is still queued and applied to the world as
normal, and only the acknowledgement for it is skipped. Nor does the ack stream
go quiet. A zone that already holds a seq for you keeps sending `world.ack` on
every broadcast tick carrying that older high-water mark, it just stops
advancing. So do not seed the counter from a nanosecond timestamp even though
`int64` holds one; the SDK also stamps `seq` through a double, which loses
precision above 2^53 before the frame leaves.

The SDK keeps no counter of its own, so keep `Seq` monotonic for the life of the
connection: a `Seq` at or below one already recorded is never acked again.

The record is per zone, not per connection. The default `view_radius` of 1 puts
you in a 3x3 interest ring, up to 9 zones at once, and each of them records the
highest `Seq` it has seen for you and acks it on its own broadcast. Two things
follow, and both are visible to any client that moves:

- More than one `world.ack` per broadcast tick, one from each subscribed zone
  holding a recorded seq for you.
- `Ack.Seq` going backwards between consecutive acks. The zone you moved away from
  stays in your ring and keeps emitting its own frozen mark. Nothing in the frame
  says which zone sent it.

So keep a running maximum and ignore any ack whose `Seq` does not exceed it. Your
own counter never goes backwards; what arrives does, and "drop everything at or
below `Ack.Seq` and replay the rest" only holds against a mark you have made
monotonic yourself. Skip that and a zone crossing re-applies inputs the server has
already consumed.

Each zone prunes its record to its own current subscribers every tick, so a mark
dies with the zone subscription rather than with the socket. A reconnect leaves no
zone holding one, so a counter restarting at 0 acks normally.

The server source and guides still call this ack "per-connection". That wording is
wrong and is tracked as
[widgrensit/asobi#477](https://github.com/widgrensit/asobi/issues/477).

### Reconciling against world.tick

`OnWorldTick(int64 Tick, const FString& UpdatesJson)` delivers a **delta**, not a
snapshot. `UpdatesJson` is the `updates` array; each entry carries an `op`:

| `op` | Meaning | Fields |
|---|---|---|
| `"a"` | Added, full state | `id` plus every field on the entity |
| `"u"` | Updated, diff | `id` plus only the changed fields |
| `"r"` | Removed | `id` only |

A new zone subscription opens with a full `"a"` snapshot of that zone's entities,
tagged tick `0`, and the frames after it are deltas. Joining subscribes you to your
whole interest ring, so a join delivers one snapshot per loaded, non-empty zone in
it, several frames rather than one. A zone holding no entities skips the entity
snapshot, but the terrain push after it is unconditional, so a world with a terrain
provider still delivers that zone's chunk on `OnWorldTerrain`.

A crossing delivers fresh snapshots too. It recomputes the ring, and every zone
that just entered it is a new subscription replaying a full snapshot. Only the
destination zone is a no-op: at `view_radius` 1 it was already in the old ring, so
resubscribing to it changes nothing. Do not read that one no-op as the whole
crossing being quiet.

Nor is a snapshot a once-per-zone event. Leaving the ring unsubscribes you, and
that zone sends an `"r"` for each of its entities on the way out. Walk back in and
you resubscribe and get another full snapshot, so a player oscillating across a
boundary re-snapshots every time.

Accumulate the deltas into your own state map: assigning `UpdatesJson` wholesale to
an "authoritative state" variable is wrong, and a map seeded only from `"u"` entries
stays empty, because your own player first arrives as an `"a"`.

When a zone's broadcast tick produced changes it sends `world.tick` first and
`world.ack` second. When nothing changed that zone sends no `world.tick` at all and
the ack arrives on its own. So prune and replay in the `OnWorldAck` handler, never
inside `OnWorldTick`: an ack that arrives with no tick beside it still has to drain
the pending buffer. The loop:

1. Increment `NextSeq`, apply the input locally, and keep it in a `TMap<int64, FMyInput>` of pending inputs (`FMyInput` is your own struct).
2. Send it with `WorldInputWithSeq`.
3. Fold every `OnWorldTick` delta into your authoritative state.
4. On `OnWorldAck`, discard the ack unless `Ack.Seq` beats your running maximum, since a lower one is a stale zone's mark. Otherwise store it as the new maximum, drop every pending input with `Seq <=` it, and re-apply the remainder in `Seq` order on top of that state.

The ack only rides broadcast ticks. A single ticker per world fans one shared tick
number out to every zone, and `broadcast_interval` is one world-level value copied
into each zone, so zones are not on independent schedules: the several acks a
multi-zone subscriber receives all land together on the same broadcast tick. It
defaults to 3 simulation ticks; set it to 1 for an ack every tick, which is what
prediction wants
([world server config](https://asobi.dev/docs/world-server)).

Binding `OnWorldAck` is not enough on its own: keep calling plain `WorldInput`
and nothing ever fires, with no error. Input sent while you are not in a zone is
dropped with no reply at all, which looks the same from the client.

Needs plugin release v1.4.0 or newer, and a server carrying `world.ack`
(asobi >= v0.84.0); an older server sends no ack rather than an error.
`AsobiSDK.uplugin` does not track the release version, its `VersionName` reads
`1.0.0` in every release, so check the source instead: `OnWorldAck` and
`WorldInputWithSeq` are in `Source/AsobiSDK/Public/AsobiWebSocket.h` from v1.4.0
onwards and absent before it.

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
