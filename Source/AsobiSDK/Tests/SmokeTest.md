# Unreal Smoke Test + AsobiCore Dispatch

Two complementary tests cover the Unreal SDK.

## `Asobi.Smoke` — end-to-end backend smoke (UE Automation, manual)

End-to-end test that proves the AsobiSDK plugin can talk to a live Asobi backend and complete the canonical 3-scenario flow:

1. Auth + WebSocket connect (two players)
2. `matchmaker.add` → both clients receive `match.matched` with the same `match_id`
3. `match.input` `{move_x:1}` → A observes a `match.state` where `players[A].x > x_initial + 10`

Spec: [widgrensit/sdk_demo_backend/SMOKE.md](https://github.com/widgrensit/sdk_demo_backend/blob/main/SMOKE.md). Mode `"demo"`. Default URL `http://localhost:8084`. Override with `ASOBI_URL` env var.

## AsobiCore dispatch (`asobi-core` CI job)

Pure unit test (no backend, no socket, no Unreal Engine) that loads every canonical asobi protocol fixture (35 server-event types) and asserts each round-trips through `asobi::core::ParseEventId` to its expected `EventId`. Catches the doc-vs-server drift class of bugs (e.g. server emits `match.matched` but the SDK only listens for `matchmaker.matched`).

This lives in `Source/AsobiCore/` as a plain C++17 doctest binary that runs on stock CI runners — no Unreal Editor required. Mirrors the dispatch tests in the love2d, godot, defold, dart, and js SDKs. Run locally with:

```bash
cmake -S Source/AsobiCore -B build
cmake --build build
cd build && ctest --output-on-failure
```

The UE-side `UAsobiWebSocket::HandleMessage` calls into `asobi::core::ParseEventId` for the dispatch decision; AsobiCore is the canonical truth for "which server events exist." Drift between the C++ enum and the fixture corpus fails the `asobi-core` job on every PR.

Fixtures are vendored under `Source/AsobiSDK/Tests/Fixtures/` from `asobi/priv/protocol/fixtures/`. The CI fixture-corpus check in `.github/workflows/smoke.yml` keeps the directory listing in sync with the canonical 35-event list as a belt-and-suspenders check next to the AsobiCore test.

## What's in this directory

| File | Role |
|---|---|
| `Source/AsobiSDK/Private/Tests/AsobiSmokeTest.cpp` | UE Automation entry point (`Asobi.Smoke`) — runs from the editor or `UE5Editor-Cmd`. |
| `Source/AsobiSDK/Public/AsobiSmokeTest.h`, `Source/AsobiSDK/Private/AsobiSmokeTest.cpp` | `UAsobiSmokeTest` UObject — the actual test runner, callable from C++ or Blueprint. |
| `Source/AsobiCore/Tests/DispatchTest.cpp` | doctest dispatch test — runs in the `asobi-core` CI job. |
| `Source/AsobiSDK/Tests/Fixtures/*.json` | Canonical server-event fixtures (35). |

The Automation test is a thin wrapper that drives `UAsobiSmokeTest` and asserts on its `OnResult` delegate.

## Running

### 1. Bring up the backend

```bash
git clone https://github.com/widgrensit/sdk_demo_backend
cd sdk_demo_backend && docker compose up -d
```

The backend listens on `http://localhost:8084` (HTTP + WebSocket on `/ws`).

### 2. Run the test

#### Option A — UE Automation (recommended)

In any UE 5.4+ project that has the `AsobiSDK` plugin enabled:

```bash
UE5Editor-Cmd /path/to/MyProject.uproject \
  -ExecCmds="Automation RunTests Asobi.Smoke; Quit" \
  -unattended -nullrhi -log
```

Or open the editor → *Tools → Session Frontend → Automation* → check `Asobi.Smoke` → *Start Tests*. The dispatch unit test lives in AsobiCore and runs on stock CI — no editor needed.

To target a remote backend:

```bash
ASOBI_URL=https://staging.example.com \
UE5Editor-Cmd MyProject.uproject \
  -ExecCmds="Automation RunTests Asobi.Smoke; Quit" \
  -unattended -nullrhi -log
```

#### Option B — `UAsobiSmokeTest` from Blueprint or C++

```cpp
UAsobiSmokeTest* Smoke = NewObject<UAsobiSmokeTest>(this);
Smoke->OnResult.AddDynamic(this, &AMyActor::HandleSmokeResult);
Smoke->RunTest(TEXT("http://localhost:8084"));
```

```cpp
UFUNCTION()
void HandleSmokeResult(bool bOk, const FString& Msg)
{
    UE_LOG(LogTemp, Log, TEXT("[smoke] %s — %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *Msg);
}
```

## CI status

The companion workflow `.github/workflows/smoke.yml` does **not** run the UE Automation test in GitHub Actions. Running a real UE test on a hosted runner would require:

- An installed Unreal Engine (multi-GB binaries, license-restricted).
- A host UE project that includes this plugin.
- Sustained build time (~30–60 min cold) per run on a public runner.

Instead, CI exercises:

- **AsobiCore dispatch** — builds the engine-agnostic `Source/AsobiCore/` library with cmake + g++, runs its doctest suite under ctest. Every fixture must round-trip through `ParseEventId`, every `EventId` must have a fixture. This is the real dispatch gate.
- **Smoke source syntax** — `clang-format` parse check + `clang -fsyntax-only` against a UE-stub header so the smoke file at least parses as C++.
- **Backend wire-protocol contract** — boots `widgrensit/sdk_demo_backend` via Docker and hits `POST /api/v1/auth/register` plus opens a `/ws` handshake. This proves the contract the plugin is coded against still works, end-to-end against the real server.

A self-hosted runner with UE pre-installed would close the remaining gap (`Asobi.Smoke` end-to-end); until then the Automation smoke test is run manually before each release.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `register failed for A/B` | Backend not running or not reachable on `ASOBI_URL`. Run `curl http://localhost:8084/api/v1/auth/register -X POST` to confirm. |
| `match_id mismatch` | Two separate matches were formed. Check `mode` is `"demo"` (the default). |
| `timeout: smoke test did not finish within 30s` | Usually a stalled WS connect. Check `docker compose logs asobi`. |
| First `match.state` arrives but x never advances | `match.input` not reaching the server. Confirm both clients made it to `match.matched` with the same `match_id`. |
