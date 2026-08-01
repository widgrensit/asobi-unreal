// UE Automation test for the game.error dispatch path (widgrensit/asobi#238).
//
// game.error is a dev-mode-only server push, sent to a player when a Lua
// game-script callback throws while handling their input. Only emitted when
// the backend runs with ASOBI_DEV_ERRORS=true; production keeps script
// errors server-side. See asobi/guides/websocket-protocol.md.
//
// This feeds the exact canonical fixture (mirrors
// Source/AsobiSDK/Tests/Fixtures/game.error.json byte-for-byte) through
// UAsobiWebSocket::HandleMessageForTest and asserts OnGameError fires with
// the Callback/Script/Message fields parsed out correctly.
//
// Run (needs an installed Unreal Engine - not available on stock CI):
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.GameError; Quit" \
//     -unattended -nullrhi -log
//
// The engine-agnostic wire-type -> EventId mapping (i.e. that "game.error"
// resolves to EventId::GameError at all) is covered on stock CI by
// AsobiCore's doctest dispatch test (Source/AsobiCore/Tests/DispatchTest.cpp).
// This test covers the UE-side field extraction and delegate broadcast that
// AsobiCore intentionally does not reach into.

#include "AsobiWebSocket.h"
#include "AsobiGameErrorAutomationProxy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsobiGameErrorTest,
	"Asobi.GameError",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

bool FAsobiGameErrorTest::RunTest(const FString& Parameters)
{
	UAsobiWebSocket* Socket = NewObject<UAsobiWebSocket>();
	UAsobiGameErrorAutomationProxy* Proxy = NewObject<UAsobiGameErrorAutomationProxy>();
	Socket->AddToRoot();
	Proxy->AddToRoot();

	Socket->OnGameError.AddDynamic(Proxy, &UAsobiGameErrorAutomationProxy::HandleGameError);

	// Verbatim copy of Source/AsobiSDK/Tests/Fixtures/game.error.json.
	const FString Fixture = TEXT(
		"{\"type\":\"game.error\",\"payload\":{\"callback\":\"handle_input\",\"script\":\"match.lua\",\"message\":\"bad arithmetic + on nil, 1\"}}");

	Socket->HandleMessageForTest(Fixture);

	TestTrue(TEXT("OnGameError fired"), Proxy->bFired);
	TestEqual(TEXT("Callback field"), Proxy->LastError.Callback, FString(TEXT("handle_input")));
	TestEqual(TEXT("Script field"), Proxy->LastError.Script, FString(TEXT("match.lua")));
	TestEqual(TEXT("Message field"), Proxy->LastError.Message, FString(TEXT("bad arithmetic + on nil, 1")));

	Proxy->RemoveFromRoot();
	Socket->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
