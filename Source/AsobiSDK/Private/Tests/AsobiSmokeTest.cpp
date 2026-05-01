// UE Automation entrypoint for the canonical Asobi smoke flow.
//
// This test wraps UAsobiSmokeTest (the engine-driven UObject runner) so it can
// be launched from the editor or command line via:
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.Smoke; Quit" \
//     -unattended -nullrhi -log
//
// Backend bring-up (must be running BEFORE invoking this test):
//
//   git clone https://github.com/widgrensit/sdk_demo_backend
//   cd sdk_demo_backend && docker compose up -d
//
// The test reads ASOBI_URL from the environment and defaults to
// http://localhost:8084 (the sdk_demo_backend default).
//
// See Source/AsobiSDK/Tests/SmokeTest.md and the canonical spec at
// https://github.com/widgrensit/sdk_demo_backend/blob/main/SMOKE.md.

#include "AsobiSmokeTest.h"
#include "AsobiSmokeAutomationProxy.h"

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AsobiSmokeAutomationDetail
{
	struct FRunner
	{
		TStrongObjectPtr<UAsobiSmokeTest> Smoke;
		TStrongObjectPtr<UAsobiSmokeAutomationProxy> Proxy;
		double DeadlineSeconds = 0.0;
	};

	static FString ResolveBackendUrl()
	{
		FString FromEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("ASOBI_URL"));
		if (!FromEnv.IsEmpty())
		{
			return FromEnv;
		}
		return TEXT("http://localhost:8084");
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FAsobiSmokeWaitCommand,
	TSharedPtr<AsobiSmokeAutomationDetail::FRunner>, Runner);

bool FAsobiSmokeWaitCommand::Update()
{
	if (!Runner.IsValid() || !Runner->Proxy.IsValid())
	{
		return true;
	}

	if (Runner->Proxy->bDone)
	{
		return true;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now >= Runner->DeadlineSeconds)
	{
		Runner->Proxy->bDone = true;
		Runner->Proxy->bOk = false;
		Runner->Proxy->LastMessage = TEXT("smoke wait command timeout (45s)");
		return true;
	}

	return false; // keep ticking
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsobiSmokeTest,
	"Asobi.Smoke",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

bool FAsobiSmokeTest::RunTest(const FString& Parameters)
{
	using namespace AsobiSmokeAutomationDetail;

	const FString Url = ResolveBackendUrl();
	UE_LOG(LogTemp, Log, TEXT("[smoke] Asobi.Smoke automation against %s"), *Url);

	UWorld* World = nullptr;
	if (GEngine && GEngine->GetWorldContexts().Num() > 0)
	{
		World = GEngine->GetWorldContexts()[0].World();
	}
	if (!World)
	{
		AddError(TEXT("no UWorld available; run inside the editor or PIE"));
		return false;
	}

	TSharedPtr<FRunner> Runner = MakeShared<FRunner>();
	Runner->Smoke = TStrongObjectPtr<UAsobiSmokeTest>(NewObject<UAsobiSmokeTest>(World));
	Runner->Proxy = TStrongObjectPtr<UAsobiSmokeAutomationProxy>(
		NewObject<UAsobiSmokeAutomationProxy>(World));
	Runner->DeadlineSeconds = FPlatformTime::Seconds() + 45.0;

	Runner->Smoke->OnResult.AddDynamic(
		Runner->Proxy.Get(),
		&UAsobiSmokeAutomationProxy::HandleResult);

	Runner->Smoke->RunTest(Url);

	ADD_LATENT_AUTOMATION_COMMAND(FAsobiSmokeWaitCommand(Runner));

	// Final assertion runs after the latent command resolves.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
		[this, Runner]()
		{
			if (!Runner->Proxy.IsValid() || !Runner->Proxy->bOk)
			{
				const FString Msg = Runner->Proxy.IsValid()
					? Runner->Proxy->LastMessage
					: TEXT("proxy lost before completion");
				AddError(FString::Printf(TEXT("smoke failed: %s"), *Msg));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[smoke] %s"), *Runner->Proxy->LastMessage);
			}
			return true;
		}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
