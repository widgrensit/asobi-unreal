#include "AsobiSmokeTest.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UAsobiSmokeTest::UAsobiSmokeTest()
{
}

void UAsobiSmokeTest::RunTest(const FString& InBaseUrl)
{
	bFinished = false;
	bAQueued = false;
	bBQueued = false;
	bInputSent = false;
	bXInitialCaptured = false;
	XInitial = 0.0f;

	BaseUrl = InBaseUrl;
	WsUrl = DeriveWsUrl(InBaseUrl);

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

	const FString Pw = TEXT("smoke_pw_12345");
	const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
	const FString UserA = FString::Printf(TEXT("smoke_a_%lld_%d"), Now, FMath::Rand());
	const FString UserB = FString::Printf(TEXT("smoke_b_%lld_%d"), Now, FMath::Rand());

	UE_LOG(LogTemp, Log, TEXT("[smoke] Backend URL: %s (ws: %s)"), *BaseUrl, *WsUrl);
	UE_LOG(LogTemp, Log, TEXT("[smoke] Scenario 1: register A=%s, B=%s"), *UserA, *UserB);

	FOnAsobiAuthResponse OnA;
	OnA.BindDynamic(this, &UAsobiSmokeTest::HandleAuthResultA);
	A.Auth->Register(UserA, Pw, UserA, OnA);

	FOnAsobiAuthResponse OnB;
	OnB.BindDynamic(this, &UAsobiSmokeTest::HandleAuthResultB);
	B.Auth->Register(UserB, Pw, UserB, OnB);
}

void UAsobiSmokeTest::SetupPlayer(FPlayer& P, const FString& InBaseUrl, bool bIsA)
{
	P.Client = NewObject<UAsobiClient>(this);
	P.Client->SetBaseUrl(InBaseUrl);

	P.Auth = NewObject<UAsobiAuth>(this);
	P.Auth->Init(P.Client);

	P.Matchmaker = NewObject<UAsobiMatchmaker>(this);
	P.Matchmaker->Init(P.Client);

	P.WebSocket = NewObject<UAsobiWebSocket>(this);
	if (bIsA)
	{
		P.WebSocket->OnConnected.AddDynamic(this, &UAsobiSmokeTest::HandleWsConnectedA);
		P.WebSocket->OnMatchMatched.AddDynamic(this, &UAsobiSmokeTest::HandleMatchEventA);
		P.WebSocket->OnMatchState.AddDynamic(this, &UAsobiSmokeTest::HandleMatchStateA);
	}
	else
	{
		P.WebSocket->OnConnected.AddDynamic(this, &UAsobiSmokeTest::HandleWsConnectedB);
		P.WebSocket->OnMatchMatched.AddDynamic(this, &UAsobiSmokeTest::HandleMatchEventB);
	}
}

void UAsobiSmokeTest::HandleAuthResultA(bool bSuccess, const FAsobiAuthTokens& Tokens, const FAsobiError& Error)
{
	if (!bSuccess)
	{
		Finish(false, FString::Printf(TEXT("register failed for A: %d %s"), Error.StatusCode, *Error.Reason));
		return;
	}
	A.PlayerId = Tokens.PlayerId;
	UE_LOG(LogTemp, Log, TEXT("[smoke] Registered A: %s"), *A.PlayerId);
	A.WebSocket->Connect(WsUrl);
}

void UAsobiSmokeTest::HandleAuthResultB(bool bSuccess, const FAsobiAuthTokens& Tokens, const FAsobiError& Error)
{
	if (!bSuccess)
	{
		Finish(false, FString::Printf(TEXT("register failed for B: %d %s"), Error.StatusCode, *Error.Reason));
		return;
	}
	B.PlayerId = Tokens.PlayerId;
	UE_LOG(LogTemp, Log, TEXT("[smoke] Registered B: %s"), *B.PlayerId);
	B.WebSocket->Connect(WsUrl);
}

void UAsobiSmokeTest::HandleWsConnectedA()
{
	UE_LOG(LogTemp, Log, TEXT("[smoke] A WS connected"));
	A.WebSocket->Authenticate(A.Client->GetAuthToken());
	if (!bAQueued)
	{
		bAQueued = true;
		UE_LOG(LogTemp, Log, TEXT("[smoke] Scenario 2: A matchmaker.add mode=%s"), *MatchMode);
		A.Matchmaker->Add(MatchMode, TArray<FString>{ A.PlayerId },
			FOnAsobiMatchmakerResponse::CreateLambda([](bool, const FAsobiMatchmakerTicket&) {}));
	}
}

void UAsobiSmokeTest::HandleWsConnectedB()
{
	UE_LOG(LogTemp, Log, TEXT("[smoke] B WS connected"));
	B.WebSocket->Authenticate(B.Client->GetAuthToken());
	if (!bBQueued)
	{
		bBQueued = true;
		UE_LOG(LogTemp, Log, TEXT("[smoke] Scenario 2: B matchmaker.add mode=%s"), *MatchMode);
		B.Matchmaker->Add(MatchMode, TArray<FString>{ B.PlayerId },
			FOnAsobiMatchmakerResponse::CreateLambda([](bool, const FAsobiMatchmakerTicket&) {}));
	}
}

void UAsobiSmokeTest::HandleMatchEventA(const FString& PayloadJson)
{
	if (A.bMatched) return;
	A.bMatched = true;
	A.MatchedMatchId = ExtractMatchId(PayloadJson);
	UE_LOG(LogTemp, Log, TEXT("[smoke] A matched: %s"), *A.MatchedMatchId);
	CheckMatchedBoth();
}

void UAsobiSmokeTest::HandleMatchEventB(const FString& PayloadJson)
{
	if (B.bMatched) return;
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
	UE_LOG(LogTemp, Log, TEXT("[smoke] Scenario 3: both matched on %s, awaiting first match.state"),
		*A.MatchedMatchId);
}

void UAsobiSmokeTest::HandleMatchStateA(const FString& StateJson)
{
	if (bFinished) return;
	if (!A.bMatched || !B.bMatched) return;

	const float X = ExtractPlayerX(StateJson, A.PlayerId);
	if (X < 0.f) return; // player not present in this state frame yet

	if (!bXInitialCaptured)
	{
		bXInitialCaptured = true;
		XInitial = X;
		UE_LOG(LogTemp, Log, TEXT("[smoke] x_initial = %.2f"), XInitial);

		if (!bInputSent)
		{
			bInputSent = true;
			UE_LOG(LogTemp, Log, TEXT("[smoke] sending match.input move_x=1"));
			A.WebSocket->SendMatchInput(TEXT("{\"move_x\":1,\"move_y\":0}"));
		}
		return;
	}

	if (X > XInitial + MinXAdvancePx)
	{
		UE_LOG(LogTemp, Log, TEXT("[smoke] match.state x=%.2f > x_initial+%.1f (=%.2f)"),
			X, MinXAdvancePx, XInitial + MinXAdvancePx);
		Finish(true, FString::Printf(TEXT("PASS — x_initial=%.2f x=%.2f"), XInitial, X));
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

FString UAsobiSmokeTest::DeriveWsUrl(const FString& InBaseUrl) const
{
	FString Url = InBaseUrl;
	if (Url.StartsWith(TEXT("https://")))
	{
		Url = TEXT("wss://") + Url.RightChop(8);
	}
	else if (Url.StartsWith(TEXT("http://")))
	{
		Url = TEXT("ws://") + Url.RightChop(7);
	}
	while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
	return Url + TEXT("/ws");
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
