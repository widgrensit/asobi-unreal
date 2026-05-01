# Unreal Smoke Test

End-to-end test that proves the AsobiSDK plugin can talk to a live Asobi backend and complete the canonical 3-scenario flow:

1. Auth + WebSocket connect (two players)
2. `matchmaker.add` → both clients receive `match.matched` with the same `match_id`
3. `match.input` `{move_x:1}` → A observes a `match.state` where `players[A].x > x_initial + 10`

Spec: [widgrensit/sdk_demo_backend/SMOKE.md](https://github.com/widgrensit/sdk_demo_backend/blob/main/SMOKE.md). Mode `"demo"`. Default URL `http://localhost:8084`. Override with `ASOBI_URL` env var.

## What's in this directory

| File | Role |
|---|---|
| `Source/AsobiSDK/Private/Tests/AsobiSmokeTest.cpp` | UE Automation entry point (`Asobi.Smoke`) — runs from the editor or `UE5Editor-Cmd`. |
| `Source/AsobiSDK/Public/AsobiSmokeTest.h`, `Source/AsobiSDK/Private/AsobiSmokeTest.cpp` | `UAsobiSmokeTest` UObject — the actual test runner, callable from C++ or Blueprint. |

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

Or open the editor → *Tools → Session Frontend → Automation* → check `Asobi.Smoke` → *Start Tests*.

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

- **Smoke source syntax** — `clang-format` parse check + `g++ -fsyntax-only` against a UE-stub header so the file at least parses as C++.
- **Backend wire-protocol contract** — boots `widgrensit/sdk_demo_backend` via Docker and hits `POST /api/v1/auth/register` plus opens a `/ws` handshake. This proves the contract the plugin is coded against still works, end-to-end against the real server.

A self-hosted runner with UE pre-installed would close the gap; until then the Automation test is run manually before each release.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `register failed for A/B` | Backend not running or not reachable on `ASOBI_URL`. Run `curl http://localhost:8084/api/v1/auth/register -X POST` to confirm. |
| `match_id mismatch` | Two separate matches were formed. Check `mode` is `"demo"` (the default). |
| `timeout: smoke test did not finish within 30s` | Usually a stalled WS connect. Check `docker compose logs asobi`. |
| First `match.state` arrives but x never advances | `match.input` not reaching the server. Confirm both clients made it to `match.matched` with the same `match_id`. |
