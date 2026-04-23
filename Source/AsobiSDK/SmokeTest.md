# Unreal smoke test

`UAsobiSmokeTest` exercises the 3 canonical scenarios (auth + WS, matchmaker → match.matched, input → state) against the [asobi-test-harness](https://github.com/widgrensit/asobi-test-harness).

It's a `BlueprintType` UObject so you can drive it from C++, Blueprint, or both.

## Running locally

1. Start the harness:

   ```bash
   git clone https://github.com/widgrensit/asobi-test-harness.git
   cd asobi-test-harness && docker compose up -d
   ```

2. In any UE 5.7+ project that has the `AsobiSDK` plugin enabled, drop this into any Actor's `BeginPlay`:

   ```cpp
   UAsobiSmokeTest* Smoke = NewObject<UAsobiSmokeTest>(this);
   Smoke->OnResult.AddDynamic(this, &AMyActor::HandleSmokeResult);
   Smoke->RunTest(TEXT("http://localhost:8080"));
   ```

   ```cpp
   UFUNCTION()
   void HandleSmokeResult(bool bOk, const FString& Msg)
   {
       UE_LOG(LogTemp, Log, TEXT("[smoke] %s — %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *Msg);
   }
   ```

3. Hit Play. Watch the Output Log for `[smoke]` lines.

## CI status

No automated CI yet. Options we haven't wired up:

1. **Epic's container registry + game-ci style workflow.** Requires an Epic account linked to GitHub, EULA signed, and the private image pullable. Roughly 30–60 min per run on a standard runner (image pull + plugin compile). Free on public repos (minutes are unlimited).
2. **Self-hosted Windows runner with UE 5.7 pre-installed.** Fast (~3 min per run), but infra burden.
3. **UE Automation Framework test** via `UnrealEditor -ExecCmds="Automation RunTests Asobi.Smoke"`. Same engine-install requirement as #1/#2.

The smoke test code above is parity with the other SDKs — whichever CI path lands, the test logic doesn't change.

## Canonical scenarios

See [widgrensit/asobi-test-harness/scenarios/canonical.md](https://github.com/widgrensit/asobi-test-harness/blob/main/scenarios/canonical.md).
