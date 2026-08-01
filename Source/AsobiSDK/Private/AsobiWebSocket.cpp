#include "AsobiWebSocket.h"
#include "AsobiClient.h"
#include "AsobiCore/Protocol.h"
#include "WebSocketsModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <optional>
#include <string_view>

UAsobiWebSocket::UAsobiWebSocket()
{
}

void UAsobiWebSocket::Connect(const FString& Url)
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, TEXT("ws"));

	WebSocket->OnConnected().AddLambda([this]()
	{
		OnConnected.Broadcast();
	});

	WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		OnError.Broadcast(Error);
	});

	WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		// 1008 (policy violation) is how the backend closes idle/unauthed or
		// revoked sockets. Surface these as auth-expired so callers force a
		// re-login instead of blindly reconnecting with a dead token.
		if (StatusCode == 1008
			|| Reason.Contains(TEXT("idle_auth_timeout"))
			|| Reason.Contains(TEXT("session_revoked"))
			|| Reason.Contains(TEXT("invalid_token")))
		{
			OnAuthExpired.Broadcast();
		}
		OnDisconnected.Broadcast(Reason);
	});

	WebSocket->OnMessage().AddLambda([this](const FString& Message)
	{
		HandleMessage(Message);
	});

	WebSocket->Connect();
}

void UAsobiWebSocket::Disconnect()
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		WebSocket->Close();
	}
}

bool UAsobiWebSocket::IsConnected() const
{
	return WebSocket.IsValid() && WebSocket->IsConnected();
}

void UAsobiWebSocket::Authenticate(const FString& Token)
{
	LastAuthToken = Token;
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("token"), Token);
	Send(TEXT("session.connect"), Payload);
}

void UAsobiWebSocket::Reauthenticate(const FString& NewToken)
{
	Authenticate(NewToken);
}

void UAsobiWebSocket::BindToClient(UAsobiClient* InClient)
{
	if (InClient)
	{
		InClient->OnTokenRotated.AddUniqueDynamic(this, &UAsobiWebSocket::Reauthenticate);
	}
}

void UAsobiWebSocket::SendHeartbeat()
{
	Send(TEXT("session.heartbeat"), nullptr);
}

void UAsobiWebSocket::SendMatchInput(const FString& DataJson)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);

	TSharedPtr<FJsonObject> DataObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataJson);
	if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
	{
		Payload->SetObjectField(TEXT("data"), DataObj);
	}

	Send(TEXT("match.input"), Payload);
}

void UAsobiWebSocket::JoinMatch(const FString& MatchId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("match_id"), MatchId);
	Send(TEXT("match.join"), Payload);
}

void UAsobiWebSocket::LeaveMatch()
{
	Send(TEXT("match.leave"), nullptr);
}

void UAsobiWebSocket::MatchmakerAdd(const FString& Mode, const FString& PropertiesJson, const TArray<FString>& Party)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("mode"), Mode);

	if (!PropertiesJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> PropsObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
		if (FJsonSerializer::Deserialize(Reader, PropsObj) && PropsObj.IsValid())
		{
			Payload->SetObjectField(TEXT("properties"), PropsObj);
		}
	}

	TArray<TSharedPtr<FJsonValue>> PartyArr;
	for (const FString& P : Party)
	{
		PartyArr.Add(MakeShareable(new FJsonValueString(P)));
	}
	Payload->SetArrayField(TEXT("party"), PartyArr);

	Send(TEXT("matchmaker.add"), Payload);
}

void UAsobiWebSocket::MatchmakerRemove(const FString& TicketId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("ticket_id"), TicketId);
	Send(TEXT("matchmaker.remove"), Payload);
}

void UAsobiWebSocket::ChatJoin(const FString& ChannelId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("channel_id"), ChannelId);
	Send(TEXT("chat.join"), Payload);
}

void UAsobiWebSocket::ChatLeave(const FString& ChannelId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("channel_id"), ChannelId);
	Send(TEXT("chat.leave"), Payload);
}

void UAsobiWebSocket::ChatSend(const FString& ChannelId, const FString& Content)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("channel_id"), ChannelId);
	Payload->SetStringField(TEXT("content"), Content);
	Send(TEXT("chat.send"), Payload);
}

void UAsobiWebSocket::UpdatePresence(const FString& Status)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("status"), Status);
	Send(TEXT("presence.update"), Payload);
}

void UAsobiWebSocket::CastVote(const FString& VoteId, const FString& OptionId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("vote_id"), VoteId);
	Payload->SetStringField(TEXT("option_id"), OptionId);
	Send(TEXT("vote.cast"), Payload);
}

void UAsobiWebSocket::UseVeto(const FString& VoteId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("vote_id"), VoteId);
	Send(TEXT("vote.veto"), Payload);
}

void UAsobiWebSocket::WorldList(const FString& Mode)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	if (!Mode.IsEmpty())
	{
		Payload->SetStringField(TEXT("mode"), Mode);
	}
	Send(TEXT("world.list"), Payload);
}

void UAsobiWebSocket::WorldCreate(const FString& Mode)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("mode"), Mode);
	Send(TEXT("world.create"), Payload);
}

void UAsobiWebSocket::WorldFindOrCreate(const FString& Mode)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("mode"), Mode);
	Send(TEXT("world.find_or_create"), Payload);
}

void UAsobiWebSocket::WorldJoin(const FString& WorldId)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("world_id"), WorldId);
	Send(TEXT("world.join"), Payload);
}

void UAsobiWebSocket::WorldLeave()
{
	Send(TEXT("world.leave"), nullptr);
}

void UAsobiWebSocket::WorldInput(const FString& DataJson)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	TSharedPtr<FJsonObject> DataObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataJson);
	if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
	{
		Payload->SetObjectField(TEXT("data"), DataObj);
	}
	Send(TEXT("world.input"), Payload);
}

void UAsobiWebSocket::DmSend(const FString& RecipientId, const FString& Content)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("recipient_id"), RecipientId);
	Payload->SetStringField(TEXT("content"), Content);
	Send(TEXT("dm.send"), Payload);
}

void UAsobiWebSocket::Send(const FString& Type, const TSharedPtr<FJsonObject>& Payload)
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject);
	Msg->SetStringField(TEXT("type"), Type);
	Msg->SetNumberField(TEXT("cid"), NextCid++);

	if (Payload.IsValid())
	{
		Msg->SetObjectField(TEXT("payload"), Payload);
	}
	else
	{
		Msg->SetObjectField(TEXT("payload"), MakeShareable(new FJsonObject));
	}

	WebSocket->Send(SerializeJson(Msg));
}

void UAsobiWebSocket::HandleMessage(const FString& MessageString)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageString);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		return;
	}

	FString Type;
	Json->TryGetStringField(TEXT("type"), Type);

	const TSharedPtr<FJsonObject>* PayloadObj = nullptr;
	Json->TryGetObjectField(TEXT("payload"), PayloadObj);

	FString PayloadStr;
	if (PayloadObj && PayloadObj->IsValid())
	{
		PayloadStr = SerializeJson(*PayloadObj);
	}

	// Broadcast raw message
	OnMessage.Broadcast(Type, PayloadStr);

	// Resolve the wire-`type` to an engine-agnostic EventId via AsobiCore.
	// Unknown types fall through silently (preserves prior behavior — the
	// raw OnMessage above still surfaces them to user code).
	const FTCHARToUTF8 TypeUtf8(*Type);
	const std::optional<asobi::core::EventId> Id =
		asobi::core::ParseEventId(std::string_view(TypeUtf8.Get(), TypeUtf8.Length()));
	if (!Id.has_value())
	{
		return;
	}

	using asobi::core::EventId;
	switch (*Id)
	{
	case EventId::SessionConnected:
	case EventId::SessionHeartbeat:
		// Intentionally surfaced only via the raw OnMessage above —
		// matches the per-SDK contract (love2d, godot, etc).
		break;

	case EventId::Error:
	{
		// Also surfaced via raw OnMessage above; here we additionally detect
		// auth-fatal reasons so callers can force a re-login.
		FString Reason;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("reason"), Reason);
		}
		if (Reason == TEXT("invalid_token") || Reason == TEXT("session_revoked"))
		{
			OnAuthExpired.Broadcast();
		}
		break;
	}

	case EventId::GameError:
	{
		FAsobiGameError Error;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Error = UAsobiClient::ParseGameError(*PayloadObj);
		}
		OnGameError.Broadcast(Error);
		break;
	}

	case EventId::GameMessage:
	{
		FAsobiGameMessage Message;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Message = UAsobiClient::ParseGameMessage(*PayloadObj);
		}
		OnGameMessage.Broadcast(Message);
		break;
	}

	case EventId::MatchState:
		OnMatchState.Broadcast(PayloadStr);
		break;
	case EventId::MatchJoined:
		OnMatchJoined.Broadcast(PayloadStr);
		break;
	case EventId::MatchLeft:
		OnMatchLeft.Broadcast();
		break;
	case EventId::MatchMatched:
		OnMatchMatched.Broadcast(PayloadStr);
		OnMatchEvent.Broadcast(TEXT("matched"), PayloadStr);
		break;
	case EventId::MatchList:
	case EventId::MatchFinished:
	case EventId::MatchMatchmakerExpired:
	case EventId::MatchMatchmakerFailed:
	case EventId::MatchVoteResult:
	case EventId::MatchVoteStart:
	case EventId::MatchVoteTally:
	case EventId::MatchVoteVetoed:
		OnMatchEvent.Broadcast(Type.Mid(6), PayloadStr); // strip "match."
		break;

	case EventId::MatchmakerQueued:
	{
		FString TicketId;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("ticket_id"), TicketId);
		}
		OnMatchmakerQueued.Broadcast(TicketId);
		break;
	}
	case EventId::MatchmakerRemoved:
		OnMatchmakerRemoved.Broadcast();
		break;

	case EventId::ChatMessage:
	{
		FString ChannelId;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("channel_id"), ChannelId);
		}
		OnChatReceived.Broadcast(ChannelId, PayloadStr);
		break;
	}
	case EventId::ChatJoined:
	{
		FString ChannelId;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("channel_id"), ChannelId);
		}
		OnChatJoined.Broadcast(ChannelId);
		break;
	}
	case EventId::ChatLeft:
	{
		FString ChannelId;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("channel_id"), ChannelId);
		}
		OnChatLeft.Broadcast(ChannelId);
		break;
	}

	case EventId::NotificationNew:
		OnNotificationReceived.Broadcast(PayloadStr);
		break;
	case EventId::PresenceUpdated:
	{
		FString Status;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("status"), Status);
		}
		OnPresenceUpdated.Broadcast(Status);
		break;
	}
	case EventId::VoteCastOk:
		OnVoteCastOk.Broadcast();
		break;
	case EventId::VoteVetoOk:
		OnVoteVetoOk.Broadcast();
		break;

	case EventId::WorldJoined:
	{
		FAsobiWorldInfo Info;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Info = UAsobiClient::ParseWorldInfo(*PayloadObj);
		}
		OnWorldJoined.Broadcast(Info);
		break;
	}
	case EventId::WorldLeft:
		OnWorldLeft.Broadcast();
		break;
	case EventId::WorldList:
	{
		TArray<FAsobiWorldInfo> Worlds;
		if (PayloadObj && PayloadObj->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if ((*PayloadObj)->TryGetArrayField(TEXT("worlds"), Arr))
			{
				for (const auto& Val : *Arr)
				{
					const TSharedPtr<FJsonObject>* Obj;
					if (Val->TryGetObject(Obj))
					{
						Worlds.Add(UAsobiClient::ParseWorldInfo(*Obj));
					}
				}
			}
		}
		OnWorldList.Broadcast(Worlds);
		break;
	}
	case EventId::WorldTick:
	{
		int64 Tick = 0;
		FString UpdatesJson;
		if (PayloadObj && PayloadObj->IsValid())
		{
			double TickNum = 0;
			if ((*PayloadObj)->TryGetNumberField(TEXT("tick"), TickNum))
			{
				Tick = static_cast<int64>(TickNum);
			}
			const TArray<TSharedPtr<FJsonValue>>* UpdatesArr;
			const TSharedPtr<FJsonObject>* UpdatesObj;
			if ((*PayloadObj)->TryGetArrayField(TEXT("updates"), UpdatesArr))
			{
				TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UpdatesJson);
				FJsonSerializer::Serialize(*UpdatesArr, Writer);
			}
			else if ((*PayloadObj)->TryGetObjectField(TEXT("updates"), UpdatesObj))
			{
				UpdatesJson = SerializeJson(*UpdatesObj);
			}
		}
		OnWorldTick.Broadcast(Tick, UpdatesJson);
		break;
	}
	case EventId::WorldTerrain:
	{
		FAsobiWorldTerrainChunk Chunk;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Chunk = UAsobiClient::ParseWorldTerrain(*PayloadObj);
		}
		OnWorldTerrain.Broadcast(Chunk);
		break;
	}
	case EventId::WorldPhaseChanged:
	case EventId::WorldFinished:
		OnWorldEvent.Broadcast(Type.Mid(6), PayloadStr); // strip "world."
		break;

	case EventId::DmMessage:
	{
		FAsobiDirectMessage Msg;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Msg = UAsobiClient::ParseDirectMessage(*PayloadObj);
		}
		OnDmMessage.Broadcast(Msg);
		break;
	}
	case EventId::DmSent:
	{
		FString ChannelId;
		if (PayloadObj && PayloadObj->IsValid())
		{
			(*PayloadObj)->TryGetStringField(TEXT("channel_id"), ChannelId);
		}
		OnDmSent.Broadcast(ChannelId);
		break;
	}
	}
}

FString UAsobiWebSocket::SerializeJson(const TSharedPtr<FJsonObject>& Obj)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Output;
}
