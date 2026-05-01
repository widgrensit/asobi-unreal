// Dispatch unit test for AsobiSDK.
//
// Feeds every canonical asobi protocol fixture (32 server-event types) through
// UAsobiWebSocket::HandleMessage and asserts the dispatch path fires a
// delegate. Catches the doc-vs-server drift class of bugs (e.g. server emits
// match.matched but the SDK only listens for matchmaker.matched) before any
// user reports a silent failure.
//
// This is an editor-only UE Automation Spec — it does NOT require a backend.
// Run from the editor or via:
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.Dispatch; Quit" \
//     -unattended -nullrhi -log
//
// Hosted GitHub Actions cannot run UE Automation Specs — see the smoke
// workflow comment for context. The test source still ships so a self-hosted
// UE runner (when available) has a target.

#include "AsobiWebSocket.h"
#include "AsobiDispatchAutomationProxy.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AsobiDispatchTestDetail
{
	// Resolves the directory containing the vendored protocol fixtures. The
	// AsobiSDK plugin ships them under Source/AsobiSDK/Tests/Fixtures/. Plugin
	// content lives under <Plugin>/Source/... in source builds and is
	// addressable via IPluginManager.
	static FString ResolveFixtureDir()
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AsobiSDK"));
		if (Plugin.IsValid())
		{
			return FPaths::Combine(
				Plugin->GetBaseDir(),
				TEXT("Source"), TEXT("AsobiSDK"), TEXT("Tests"), TEXT("Fixtures"));
		}
		// Fallback: relative to the project plugins dir.
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("AsobiSDK"), TEXT("Source"), TEXT("AsobiSDK"), TEXT("Tests"), TEXT("Fixtures"));
	}

	// Canonical 32-event corpus. Mirrors the asobi protocol fixture corpus in
	// asobi/priv/protocol/fixtures/ and the SERVER_EVENTS map in
	// asobi-love2d's realtime.lua. Drift between this list and the dispatch
	// table in AsobiWebSocket.cpp is what this test catches.
	static const TCHAR* const ExpectedTypes[] = {
		TEXT("chat.joined"),
		TEXT("chat.left"),
		TEXT("chat.message"),
		TEXT("dm.message"),
		TEXT("dm.sent"),
		TEXT("error"),
		TEXT("match.finished"),
		TEXT("match.joined"),
		TEXT("match.left"),
		TEXT("match.matched"),
		TEXT("match.matchmaker_expired"),
		TEXT("match.matchmaker_failed"),
		TEXT("match.state"),
		TEXT("match.vote_result"),
		TEXT("match.vote_start"),
		TEXT("match.vote_tally"),
		TEXT("match.vote_vetoed"),
		TEXT("matchmaker.queued"),
		TEXT("matchmaker.removed"),
		TEXT("notification.new"),
		TEXT("presence.updated"),
		TEXT("session.connected"),
		TEXT("session.heartbeat"),
		TEXT("vote.cast_ok"),
		TEXT("vote.veto_ok"),
		TEXT("world.finished"),
		TEXT("world.joined"),
		TEXT("world.left"),
		TEXT("world.list"),
		TEXT("world.phase_changed"),
		TEXT("world.terrain"),
		TEXT("world.tick"),
	};

	// session.connected, session.heartbeat, and error intentionally have no
	// typed delegate today — they surface only via the raw OnMessage. Anything
	// else in the corpus is expected to fire a typed delegate.
	static bool HasTypedDelegate(const FString& MType)
	{
		if (MType == TEXT("session.connected")) return false;
		if (MType == TEXT("session.heartbeat")) return false;
		if (MType == TEXT("error")) return false;
		return true;
	}

	// Mirrors the dispatch table in AsobiWebSocket.cpp. Binds the
	// type-specific delegate that should fire for `MType`.
	static void BindTypedDelegate(UAsobiWebSocket* Ws,
		UAsobiDispatchAutomationProxy* Proxy, const FString& MType)
	{
		if (MType == TEXT("match.state"))
		{
			Ws->OnMatchState.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("match.joined"))
		{
			Ws->OnMatchJoined.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("match.left"))
		{
			Ws->OnMatchLeft.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleVoid);
		}
		else if (MType == TEXT("match.matched"))
		{
			Ws->OnMatchMatched.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType.StartsWith(TEXT("match.")))
		{
			// match.finished, match.matchmaker_expired, match.matchmaker_failed,
			// match.vote_start, match.vote_tally, match.vote_result, match.vote_vetoed
			// — all fall through to OnMatchEvent.
			Ws->OnMatchEvent.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleStringString);
		}
		else if (MType == TEXT("matchmaker.queued"))
		{
			Ws->OnMatchmakerQueued.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("matchmaker.removed"))
		{
			Ws->OnMatchmakerRemoved.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleVoid);
		}
		else if (MType == TEXT("chat.message"))
		{
			Ws->OnChatReceived.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleStringString);
		}
		else if (MType == TEXT("chat.joined"))
		{
			Ws->OnChatJoined.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("chat.left"))
		{
			Ws->OnChatLeft.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("notification.new"))
		{
			Ws->OnNotificationReceived.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("presence.updated"))
		{
			Ws->OnPresenceUpdated.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
		else if (MType == TEXT("vote.cast_ok"))
		{
			Ws->OnVoteCastOk.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleVoid);
		}
		else if (MType == TEXT("vote.veto_ok"))
		{
			Ws->OnVoteVetoOk.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleVoid);
		}
		else if (MType == TEXT("world.joined"))
		{
			Ws->OnWorldJoined.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleWorldJoined);
		}
		else if (MType == TEXT("world.left"))
		{
			Ws->OnWorldLeft.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleVoid);
		}
		else if (MType == TEXT("world.list"))
		{
			Ws->OnWorldList.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleWorldList);
		}
		else if (MType == TEXT("world.tick"))
		{
			Ws->OnWorldTick.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleWorldTick);
		}
		else if (MType == TEXT("world.terrain"))
		{
			Ws->OnWorldTerrain.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleWorldTerrain);
		}
		else if (MType.StartsWith(TEXT("world.")))
		{
			// world.phase_changed, world.finished — fall through to OnWorldEvent.
			Ws->OnWorldEvent.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleStringString);
		}
		else if (MType == TEXT("dm.message"))
		{
			Ws->OnDmMessage.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleDmMessage);
		}
		else if (MType == TEXT("dm.sent"))
		{
			Ws->OnDmSent.AddDynamic(Proxy, &UAsobiDispatchAutomationProxy::HandleString);
		}
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FAsobiDispatchTest,
	"Asobi.Dispatch",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

void FAsobiDispatchTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Type : AsobiDispatchTestDetail::ExpectedTypes)
	{
		OutBeautifiedNames.Add(FString(Type));
		OutTestCommands.Add(FString(Type));
	}
}

bool FAsobiDispatchTest::RunTest(const FString& Parameters)
{
	using namespace AsobiDispatchTestDetail;

	const FString MType = Parameters;
	const FString FixturePath = FPaths::Combine(ResolveFixtureDir(), MType + TEXT(".json"));

	if (!IFileManager::Get().FileExists(*FixturePath))
	{
		AddError(FString::Printf(TEXT("fixture missing: %s"), *FixturePath));
		return false;
	}

	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FixturePath))
	{
		AddError(FString::Printf(TEXT("could not read fixture: %s"), *FixturePath));
		return false;
	}
	if (Raw.IsEmpty())
	{
		AddError(FString::Printf(TEXT("fixture empty: %s"), *FixturePath));
		return false;
	}

	TStrongObjectPtr<UAsobiWebSocket> Ws(NewObject<UAsobiWebSocket>());
	TStrongObjectPtr<UAsobiDispatchAutomationProxy> Proxy(NewObject<UAsobiDispatchAutomationProxy>());

	Ws->OnMessage.AddDynamic(Proxy.Get(), &UAsobiDispatchAutomationProxy::HandleRaw);
	BindTypedDelegate(Ws.Get(), Proxy.Get(), MType);

	Ws->HandleMessageForTest(Raw);

	if (!Proxy->bRawFired)
	{
		AddError(FString::Printf(TEXT("OnMessage did not fire for %s"), *MType));
		return false;
	}
	if (Proxy->RawType != MType)
	{
		AddError(FString::Printf(
			TEXT("envelope type mismatch: fixture %s, parsed %s"), *MType, *Proxy->RawType));
		return false;
	}
	if (!HasTypedDelegate(MType))
	{
		// session.connected, session.heartbeat, and error intentionally have no
		// typed delegate; OnMessage is the only contract. Pass with raw confirmation.
		return true;
	}
	if (!Proxy->bTypedFired)
	{
		AddError(FString::Printf(
			TEXT("typed delegate did not fire for %s"), *MType));
		return false;
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
