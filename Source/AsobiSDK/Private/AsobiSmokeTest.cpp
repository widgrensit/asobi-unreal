#include "AsobiSmokeTest.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UAsobiSmokeTest::UAsobiSmokeTest()
{
}

void UAsobiSmokeTest::RunTest(const FString& BaseUrl)
{
	bFinished = false;
	SetupPlayer(A, BaseUrl, true);
	SetupPlayer(B, BaseUrl, false);

	// Global timeout — failsafe if any step hangs.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimeoutHandle,
			FTimerDelegate::CreateLambda([this]()
			{
				if (!bFinished)
				{
					Finish(false, TEXT("timeout: smoke test did not finish within 30s"));
				}
			}),
			30.0f, false);
	}

	const FString PwA = TEXT("smoke_pw_12345");
	const FString UserA = FString::Printf(TEXT("smoke_a_%d"), FMath::Rand());
	const FString UserB = FString::Printf(TEXT("smoke_b_%d"), FMath::Rand());

	FOnAsobiAuthResponse OnA;
	OnA.BindDynamic(this, &UAsobiSmokeTest::HandleAuthResultA);
	A.Auth->Register(UserA, PwA, UserA, OnA);

	FOnAsobiAuthResponse OnB;
	OnB.BindDynamic(this, &UAsobiSmokeTest::HandleAuthResultB);
	B.Auth->Register(UserB, PwA, UserB, OnB);
}

void UAsobiSmokeTest::SetupPlayer(FPlayer& P, const FString& BaseUrl, bool bIsA)
{
	P.Client = NewObject<UAsobiClient>(this);
	P.Client->SetBaseUrl(BaseUrl);

	P.Auth = NewObject<UAsobiAuth>(this);
	P.Auth->Init(P.Client);

	P.Matchmaker = NewObject<UAsobiMatchmaker>(this);
	P.Matchmaker->Init(P.Client);

	P.WebSocket = NewObject<UAsobiWebSocket>(this);
	if (bIsA)
	{
		P.WebSocket->OnConnected.AddDynamic(this, &UAsobiSmokeTest::HandleWsConnectedA);
		P.WebSocket->OnMatchEvent.AddDynamic(this, &UAsobiSmokeTest::HandleMatchEventA);
		P.WebSocket->OnMatchState.AddDynamic(this, &UAsobiSmokeTest::HandleMatchStateA);
	}
	else
	{
		P.WebSocket->OnConnected.AddDynamic(this, &UAsobiSmokeTest::HandleWsConnectedB);
		P.WebSocket->OnMatchEvent.AddDynamic(this, &UAsobiSmokeTest::HandleMatchEventB);
	}
}

static void DoAuth(UAsobiSmokeTest::FPlayer* P, const FAsobiAuthTokens& Tokens, FString WsUrlBase)
{
	P->PlayerId = Tokens.PlayerId;
	const FString WsUrl = WsUrlBase.Replace(TEXT("http://"), TEXT("ws://"))
	                               .Replace(TEXT("https://"), TEXT("wss://"))
	                               + TEXT("/ws");
	P->WebSocket->Connect(WsUrl);
}

void UAsobiSmokeTest::HandleAuthResultA(bool bSuccess, const FAsobiAuthTokens& Tokens)
{
	if (!bSuccess) { Finish(false, TEXT("register failed for A")); return; }
	A.PlayerId = Tokens.PlayerId;
	UE_LOG(LogTemp, Log, TEXT("[smoke] Registered A: %s"), *A.PlayerId);
	A.WebSocket->Connect(TEXT("ws://localhost:8080/ws"));
}

void UAsobiSmokeTest::HandleAuthResultB(bool bSuccess, const FAsobiAuthTokens& Tokens)
{
	if (!bSuccess) { Finish(false, TEXT("register failed for B")); return; }
	B.PlayerId = Tokens.PlayerId;
	UE_LOG(LogTemp, Log, TEXT("[smoke] Registered B: %s"), *B.PlayerId);
	B.WebSocket->Connect(TEXT("ws://localhost:8080/ws"));
}

void UAsobiSmokeTest::HandleWsConnectedA()
{
	A.WebSocket->Authenticate(A.Client->GetAuthToken());
	if (!bAQueued)
	{
		bAQueued = true;
		A.Matchmaker->Add(MatchMode, TArray<FString>{ A.PlayerId },
			FOnAsobiMatchmakerResponse::CreateLambda([](bool, const FAsobiMatchmakerTicket&) {}));
	}
}

void UAsobiSmokeTest::HandleWsConnectedB()
{
	B.WebSocket->Authenticate(B.Client->GetAuthToken());
	if (!bBQueued)
	{
		bBQueued = true;
		B.Matchmaker->Add(MatchMode, TArray<FString>{ B.PlayerId },
			FOnAsobiMatchmakerResponse::CreateLambda([](bool, const FAsobiMatchmakerTicket&) {}));
	}
}

void UAsobiSmokeTest::HandleMatchEventA(const FString& Event, const FString& PayloadJson)
{
	if (Event != TEXT("matched") || A.bMatched) return;
	A.bMatched = true;
	A.MatchedMatchId = ExtractMatchId(PayloadJson);
	UE_LOG(LogTemp, Log, TEXT("[smoke] A matched: %s"), *A.MatchedMatchId);
	CheckMatchedBoth();
}

void UAsobiSmokeTest::HandleMatchEventB(const FString& Event, const FString& PayloadJson)
{
	if (Event != TEXT("matched") || B.bMatched) return;
	B.bMatched = true;
	B.MatchedMatchId = ExtractMatchId(PayloadJson);
	UE_LOG(LogTemp, Log, TEXT("[smoke] B matched: %s"), *B.MatchedMatchId);
	CheckMatchedBoth();
}

void UAsobiSmokeTest::CheckMatchedBoth()
{
	if (!A.bMatched || !B.bMatched || bInputSent) return;
	if (A.MatchedMatchId != B.MatchedMatchId)
	{
		Finish(false, FString::Printf(TEXT("match_id mismatch: %s vs %s"),
			*A.MatchedMatchId, *B.MatchedMatchId));
		return;
	}
	bInputSent = true;
	UE_LOG(LogTemp, Log, TEXT("[smoke] Both matched, sending input"));
	A.WebSocket->SendMatchInput(TEXT("{\"move_x\":1,\"move_y\":0}"));
}

void UAsobiSmokeTest::HandleMatchStateA(const FString& StateJson)
{
	if (!bInputSent || bFinished) return;
	const float X = ExtractPlayerX(StateJson, A.PlayerId);
	if (X >= 1.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[smoke] match.state x = %f"), X);
		Finish(true, FString::Printf(TEXT("PASS — x=%f"), X));
	}
}

void UAsobiSmokeTest::Finish(bool bOk, const FString& Msg)
{
	if (bFinished) return;
	bFinished = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimeoutHandle);
	}
	if (A.WebSocket) A.WebSocket->Disconnect();
	if (B.WebSocket) B.WebSocket->Disconnect();
	UE_LOG(LogTemp, Log, TEXT("[smoke] %s"), *Msg);
	OnResult.Broadcast(bOk, Msg);
}

FString UAsobiSmokeTest::ExtractMatchId(const FString& PayloadJson) const
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
	{
		FString Id;
		Json->TryGetStringField(TEXT("match_id"), Id);
		return Id;
	}
	return FString();
}

float UAsobiSmokeTest::ExtractPlayerX(const FString& StateJson, const FString& PlayerId) const
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StateJson);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return -1.f;

	const TSharedPtr<FJsonObject>* Players;
	if (!Json->TryGetObjectField(TEXT("players"), Players) || !Players->IsValid()) return -1.f;

	const TSharedPtr<FJsonObject>* Me;
	if (!(*Players)->TryGetObjectField(PlayerId, Me) || !Me->IsValid()) return -1.f;

	double X = -1.0;
	(*Me)->TryGetNumberField(TEXT("x"), X);
	return static_cast<float>(X);
}
