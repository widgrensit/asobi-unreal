// UE Automation test for the game.message dispatch path (widgrensit/asobi#264).
//
// game.message is a server push sent to a player whenever a Lua game
// script calls `game.send(player_id, message)`. Unlike game.error, this is
// emitted unconditionally in production. `message` can be any JSON value
// the script passes — a string, a number, or a nested object/array — so
// the wire envelope wraps it as `{"message": <value>}` rather than
// constraining it to a string field. See asobi/guides/websocket-protocol.md.
//
// This feeds the canonical string fixture (mirrors
// Source/AsobiSDK/Tests/Fixtures/game.message.json byte-for-byte) plus a
// numeric and a nested-object payload through
// UAsobiWebSocket::HandleMessageForTest and asserts OnGameMessage fires
// each time with the raw FJsonValue intact — no crash, no truncation, no
// silent coercion to string.
//
// Run (needs an installed Unreal Engine - not available on stock CI):
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.GameMessage; Quit" \
//     -unattended -nullrhi -log
//
// The engine-agnostic wire-type -> EventId mapping (i.e. that "game.message"
// resolves to EventId::GameMessage at all) is covered on stock CI by
// AsobiCore's doctest dispatch test (Source/AsobiCore/Tests/DispatchTest.cpp).
// This test covers the UE-side field extraction and delegate broadcast that
// AsobiCore intentionally does not reach into.

#include "AsobiWebSocket.h"
#include "AsobiGameMessageAutomationProxy.h"

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsobiGameMessageTest,
	"Asobi.GameMessage",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

bool FAsobiGameMessageTest::RunTest(const FString& Parameters)
{
	UAsobiWebSocket* Socket = NewObject<UAsobiWebSocket>();
	UAsobiGameMessageAutomationProxy* Proxy = NewObject<UAsobiGameMessageAutomationProxy>();
	Socket->AddToRoot();
	Proxy->AddToRoot();

	Socket->OnGameMessage.AddDynamic(Proxy, &UAsobiGameMessageAutomationProxy::HandleGameMessage);

	// Case 1: string payload. Verbatim copy of
	// Source/AsobiSDK/Tests/Fixtures/game.message.json.
	{
		const FString Fixture = TEXT(
			"{\"type\":\"game.message\",\"payload\":{\"message\":\"jij bent speler nummer 3\"}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("string case: OnGameMessage fired"), Proxy->bFired);
		TestTrue(TEXT("string case: Message is valid"), Proxy->LastMessage.Message.IsValid());
		if (Proxy->LastMessage.Message.IsValid())
		{
			TestTrue(TEXT("string case: Message.Type == String"),
				Proxy->LastMessage.Message->Type == EJson::String);
			TestEqual(TEXT("string case: Message.AsString"),
				Proxy->LastMessage.Message->AsString(), FString(TEXT("jij bent speler nummer 3")));
		}
		TestEqual(TEXT("string case: MessageJson"), Proxy->LastMessage.MessageJson,
			FString(TEXT("\"jij bent speler nummer 3\"")));
	}

	// Case 2: numeric payload — must not be silently coerced to a string.
	{
		Proxy->bFired = false;
		Proxy->LastMessage = FAsobiGameMessage();

		const FString Fixture = TEXT("{\"type\":\"game.message\",\"payload\":{\"message\":3}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("number case: OnGameMessage fired"), Proxy->bFired);
		TestTrue(TEXT("number case: Message is valid"), Proxy->LastMessage.Message.IsValid());
		if (Proxy->LastMessage.Message.IsValid())
		{
			TestTrue(TEXT("number case: Message.Type == Number"),
				Proxy->LastMessage.Message->Type == EJson::Number);
			TestEqual(TEXT("number case: Message.AsNumber"),
				Proxy->LastMessage.Message->AsNumber(), 3.0);
		}
		TestEqual(TEXT("number case: MessageJson"), Proxy->LastMessage.MessageJson, FString(TEXT("3")));
	}

	// Case 3: nested object payload — must round-trip in full, not truncate.
	{
		Proxy->bFired = false;
		Proxy->LastMessage = FAsobiGameMessage();

		const FString Fixture = TEXT(
			"{\"type\":\"game.message\",\"payload\":{\"message\":{\"round\":3,\"scores\":{\"a\":1,\"b\":2}}}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("object case: OnGameMessage fired"), Proxy->bFired);
		TestTrue(TEXT("object case: Message is valid"), Proxy->LastMessage.Message.IsValid());
		if (Proxy->LastMessage.Message.IsValid())
		{
			TestTrue(TEXT("object case: Message.Type == Object"),
				Proxy->LastMessage.Message->Type == EJson::Object);
			const TSharedPtr<FJsonObject> Obj = Proxy->LastMessage.Message->AsObject();
			TestTrue(TEXT("object case: AsObject valid"), Obj.IsValid());
			if (Obj.IsValid())
			{
				double Round = 0.0;
				TestTrue(TEXT("object case: has `round`"), Obj->TryGetNumberField(TEXT("round"), Round));
				TestEqual(TEXT("object case: round == 3"), Round, 3.0);

				const TSharedPtr<FJsonObject>* Scores = nullptr;
				TestTrue(TEXT("object case: has nested `scores`"), Obj->TryGetObjectField(TEXT("scores"), Scores));
				if (Scores && Scores->IsValid())
				{
					double A = 0.0, B = 0.0;
					TestTrue(TEXT("object case: scores.a present"), (*Scores)->TryGetNumberField(TEXT("a"), A));
					TestTrue(TEXT("object case: scores.b present"), (*Scores)->TryGetNumberField(TEXT("b"), B));
					TestEqual(TEXT("object case: scores.a == 1"), A, 1.0);
					TestEqual(TEXT("object case: scores.b == 2"), B, 2.0);
				}
			}
		}

		// MessageJson must round-trip the same object — re-parse rather than
		// exact-string-match since key order is not a contract.
		TestFalse(TEXT("object case: MessageJson is non-empty"), Proxy->LastMessage.MessageJson.IsEmpty());
		TSharedPtr<FJsonObject> Reparsed;
		TSharedRef<TJsonReader<>> ReparseReader = TJsonReaderFactory<>::Create(Proxy->LastMessage.MessageJson);
		TestTrue(TEXT("object case: MessageJson re-parses"),
			FJsonSerializer::Deserialize(ReparseReader, Reparsed) && Reparsed.IsValid());
		if (Reparsed.IsValid())
		{
			double Round = 0.0;
			TestTrue(TEXT("object case: MessageJson has `round`"), Reparsed->TryGetNumberField(TEXT("round"), Round));
			TestEqual(TEXT("object case: MessageJson round == 3"), Round, 3.0);
		}
	}

	// Case 4: `message` key entirely absent from the payload object. Message
	// must stay an invalid (null) TSharedPtr and MessageJson must stay empty
	// — distinct from case 5's explicit null.
	{
		Proxy->bFired = false;
		Proxy->LastMessage = FAsobiGameMessage();

		const FString Fixture = TEXT("{\"type\":\"game.message\",\"payload\":{}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("absent case: OnGameMessage fired"), Proxy->bFired);
		TestFalse(TEXT("absent case: Message is invalid"), Proxy->LastMessage.Message.IsValid());
		TestTrue(TEXT("absent case: MessageJson is empty"), Proxy->LastMessage.MessageJson.IsEmpty());
	}

	// Case 5: `message` explicitly present as JSON null. Unlike the absent
	// case, Message is a *valid* TSharedPtr whose Type is EJson::Null, and
	// MessageJson carries the literal text "null".
	{
		Proxy->bFired = false;
		Proxy->LastMessage = FAsobiGameMessage();

		const FString Fixture = TEXT("{\"type\":\"game.message\",\"payload\":{\"message\":null}}");
		Socket->HandleMessageForTest(Fixture);

		TestTrue(TEXT("null case: OnGameMessage fired"), Proxy->bFired);
		TestTrue(TEXT("null case: Message is valid"), Proxy->LastMessage.Message.IsValid());
		if (Proxy->LastMessage.Message.IsValid())
		{
			TestTrue(TEXT("null case: Message.Type == Null"),
				Proxy->LastMessage.Message->Type == EJson::Null);
		}
		TestEqual(TEXT("null case: MessageJson == \"null\""),
			Proxy->LastMessage.MessageJson, FString(TEXT("null")));
	}

	Proxy->RemoveFromRoot();
	Socket->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
