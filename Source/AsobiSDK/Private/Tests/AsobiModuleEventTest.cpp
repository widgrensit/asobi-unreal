// UE Automation test for the module.event dispatch path.
//
// module.event is a named server push from an extension, shaped as
// `{"module": ..., "event": ..., "data": {...}}`. Unlike game.message it has
// no game.* twin - it is a single frame. The whole payload is surfaced and the
// app routes on the inner `event` name, which is data rather than a dispatch
// gate: an unfamiliar event name must still arrive intact. See
// asobi/guides/websocket-protocol.md.
//
// This feeds the canonical fixture (mirrors
// Source/AsobiSDK/Tests/Fixtures/module.event.json byte-for-byte) plus an
// unfamiliar-event payload through UAsobiWebSocket::HandleMessageForTest and
// asserts OnModuleEvent fires each time with module, event, and data intact.
//
// Run (needs an installed Unreal Engine - not available on stock CI):
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.ModuleEvent; Quit" \
//     -unattended -nullrhi -log
//
// The engine-agnostic wire-type -> EventId mapping (i.e. that "module.event"
// resolves to EventId::ModuleEvent regardless of the inner event name) is
// covered on stock CI by AsobiCore's doctest dispatch test
// (Source/AsobiCore/Tests/DispatchTest.cpp). This test covers the UE-side
// field extraction and delegate broadcast that AsobiCore does not reach into.

#include "AsobiWebSocket.h"
#include "AsobiModuleEventAutomationProxy.h"

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsobiModuleEventTest,
	"Asobi.ModuleEvent",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

bool FAsobiModuleEventTest::RunTest(const FString& Parameters)
{
	UAsobiWebSocket* Socket = NewObject<UAsobiWebSocket>();
	UAsobiModuleEventAutomationProxy* Proxy = NewObject<UAsobiModuleEventAutomationProxy>();
	Socket->AddToRoot();
	Proxy->AddToRoot();

	Socket->OnModuleEvent.AddDynamic(Proxy, &UAsobiModuleEventAutomationProxy::HandleModuleEvent);

	// Case 1: the canonical fixture. Verbatim copy of
	// Source/AsobiSDK/Tests/Fixtures/module.event.json.
	{
		const FString Fixture = TEXT(
			"{\"type\":\"module.event\",\"payload\":{\"module\":\"quests\",\"event\":\"quests.completed\","
			"\"data\":{\"quest_id\":\"01j8x000000000000000000042\",\"reward\":250}}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("known case: OnModuleEvent fired"), Proxy->bFired);
		TestEqual(TEXT("known case: Module"), Proxy->LastEvent.Module, FString(TEXT("quests")));
		TestEqual(TEXT("known case: Event"), Proxy->LastEvent.Event, FString(TEXT("quests.completed")));

		// DataJson must round-trip the object whole - re-parse rather than
		// exact-string-match since key order is not a contract.
		TestFalse(TEXT("known case: DataJson non-empty"), Proxy->LastEvent.DataJson.IsEmpty());
		TSharedPtr<FJsonObject> Reparsed;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Proxy->LastEvent.DataJson);
		TestTrue(TEXT("known case: DataJson re-parses"),
			FJsonSerializer::Deserialize(Reader, Reparsed) && Reparsed.IsValid());
		if (Reparsed.IsValid())
		{
			FString QuestId;
			TestTrue(TEXT("known case: data.quest_id present"),
				Reparsed->TryGetStringField(TEXT("quest_id"), QuestId));
			TestEqual(TEXT("known case: data.quest_id"), QuestId,
				FString(TEXT("01j8x000000000000000000042")));
			double Reward = 0.0;
			TestTrue(TEXT("known case: data.reward present"),
				Reparsed->TryGetNumberField(TEXT("reward"), Reward));
			TestEqual(TEXT("known case: data.reward == 250"), Reward, 250.0);
		}
	}

	// Case 2: an unfamiliar inner event name. The wire freeze means the inner
	// `event` is data, not a dispatch gate - so this must STILL surface, not be
	// dropped for being an event this SDK build has never heard of.
	{
		Proxy->bFired = false;
		Proxy->LastEvent = FAsobiModuleEvent();

		const FString Fixture = TEXT(
			"{\"type\":\"module.event\",\"payload\":{\"module\":\"mystery\",\"event\":\"mystery.happened\","
			"\"data\":{\"n\":7}}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("unknown case: OnModuleEvent fired"), Proxy->bFired);
		TestEqual(TEXT("unknown case: Module"), Proxy->LastEvent.Module, FString(TEXT("mystery")));
		TestEqual(TEXT("unknown case: Event"), Proxy->LastEvent.Event, FString(TEXT("mystery.happened")));
		TestFalse(TEXT("unknown case: DataJson non-empty"), Proxy->LastEvent.DataJson.IsEmpty());
	}

	Proxy->RemoveFromRoot();
	Socket->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
