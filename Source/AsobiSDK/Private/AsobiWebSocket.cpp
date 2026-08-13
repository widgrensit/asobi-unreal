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

void UAsobiWebSocket::WorldInputWithSeq(const FString& DataJson, int64 Seq)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	TSharedPtr<FJsonObject> DataObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataJson);
	if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
	{
		Payload->SetObjectField(TEXT("data"), DataObj);
	}
	SendWithCid(TEXT("world.input"), Payload, TOptional<int64>(Seq));
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
	SendWithCid(Type, Payload);
}

FString UAsobiWebSocket::SendWithCid(const FString& Type, const TSharedPtr<FJsonObject>& Payload,
                                     const TOptional<int64>& Seq)
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		return FString();
	}

	// A string, matching the documented protocol and every other SDK. This was
	// a number, which nothing read - correlation did not exist here until RPC.
	// Numbers also round-trip badly for correlation: SetNumberField takes a
	// double, so a cid can come back formatted differently from how it went
	// out and stop matching the call that is waiting on it.
	const FString Cid = FString::Printf(TEXT("c-%d"), NextCid++);

	TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject);
	Msg->SetStringField(TEXT("type"), Type);
	Msg->SetStringField(TEXT("cid"), Cid);

	// A per-input sequence for world.ack reconciliation. Rides as a top-level
	// sibling of payload, never nested, and only when provided.
	if (Seq.IsSet())
	{
		Msg->SetNumberField(TEXT("seq"), static_cast<double>(Seq.GetValue()));
	}

	if (Payload.IsValid())
	{
		Msg->SetObjectField(TEXT("payload"), Payload);
	}
	else
	{
		Msg->SetObjectField(TEXT("payload"), MakeShareable(new FJsonObject));
	}

	WebSocket->Send(SerializeJson(Msg));

	// The raw token as it will come back, so the reply matches without
	// re-quoting at every comparison.
	return FString::Printf(TEXT("\"%s\""), *Cid);
}

void UAsobiWebSocket::Rpc(const FString& Method, const TSharedPtr<FJsonObject>& Params,
                          FAsobiRpcCallback Callback)
{
	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	// protocol versions the payload rather than the frame type, so a future
	// version is a rejection a client can read.
	Payload->SetNumberField(TEXT("protocol"), 1);
	Payload->SetStringField(TEXT("method"), Method);
	Payload->SetObjectField(TEXT("params"),
		Params.IsValid() ? Params : MakeShareable(new FJsonObject));

	const FString Cid = SendWithCid(TEXT("rpc.call"), Payload);
	if (Cid.IsEmpty())
	{
		// Not connected. Answer anyway rather than leave the caller waiting on
		// a reply that was never sent - a latent Blueprint node that never
		// completes is a hang with no error to show.
		if (Callback)
		{
			FAsobiRpcError Error;
			Error.Code = TEXT("not_connected");
			Error.Message = TEXT("The realtime socket is not connected.");
			Error.DetailsJson = TEXT("{}");
			Callback(FString(), &Error);
		}
		return;
	}

	if (Callback)
	{
		PendingRpc.Add(Cid, MoveTemp(Callback));
	}
}

bool UAsobiWebSocket::RouteRpcReply(const FString& Type, const FString& MessageString)
{
	if (Type != TEXT("rpc.ok") && Type != TEXT("rpc.error"))
	{
		return false;
	}

	const FTCHARToUTF8 RawUtf8(*MessageString);
	const std::string_view RawView(RawUtf8.Get(), RawUtf8.Length());

	const std::optional<std::string> Cid = asobi::core::ParseEnvelopeCid(RawView);
	if (!Cid.has_value())
	{
		return true;
	}

	FAsobiRpcCallback Callback;
	if (!PendingRpc.RemoveAndCopyValue(UTF8_TO_TCHAR(Cid->c_str()), Callback) || !Callback)
	{
		// A reply to a call nobody is waiting on - a duplicate, or one already
		// answered. Swallow it rather than firing a callback twice.
		return true;
	}

	const std::optional<asobi::core::RpcReply> Reply = asobi::core::ParseRpcReply(RawView);
	if (!Reply.has_value())
	{
		FAsobiRpcError Error;
		Error.Code = TEXT("internal");
		Error.Message = TEXT("The server sent a malformed RPC reply.");
		Error.DetailsJson = TEXT("{}");
		Callback(FString(), &Error);
		return true;
	}

	if (Reply->bIsError)
	{
		FAsobiRpcError Error;
		Error.Code = UTF8_TO_TCHAR(Reply->Error.Code.c_str());
		Error.Message = UTF8_TO_TCHAR(Reply->Error.Message.c_str());
		Error.DetailsJson = UTF8_TO_TCHAR(Reply->Error.DetailsJson.c_str());
		Callback(FString(), &Error);
	}
	else
	{
		Callback(UTF8_TO_TCHAR(Reply->ResultJson.c_str()), nullptr);
	}
	return true;
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

	// RPC replies go to the one caller waiting on them, not to every listener,
	// so they are routed before the by-type dispatch below and never reach it.
	if (RouteRpcReply(Type, MessageString))
	{
		return;
	}

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

	// module.* are the server's current names for the same two events. Every
	// other SDK routes both pairs to the same delegate; without these a game
	// silently drops dev-console output from a server on the newer naming.
	case EventId::ModuleError:
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

	case EventId::ModuleMessage:
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

	// module.event has no game.* twin, so it stands alone. The whole payload
	// is surfaced and the app routes on the inner Event - an unfamiliar event
	// name is data, not a dispatch gate, so it still reaches the app here.
	case EventId::ModuleEvent:
	{
		FAsobiModuleEvent Event;
		if (PayloadObj && PayloadObj->IsValid())
		{
			Event = UAsobiClient::ParseModuleEvent(*PayloadObj);
		}
		OnModuleEvent.Broadcast(Event);
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
	// world.ack carries the highest world.input seq the sending zone has consumed
	// for this player as of tick. Per zone, not per connection
	// (widgrensit/asobi#477), so seq is not monotonic across acks. Sent only to
	// clients that stamped a seq on their input - it never rides the shared
	// world.tick.
	case EventId::WorldAck:
	{
		FAsobiWorldAck Ack;
		if (PayloadObj && PayloadObj->IsValid())
		{
			double TickNum = 0;
			if ((*PayloadObj)->TryGetNumberField(TEXT("tick"), TickNum))
			{
				Ack.Tick = static_cast<int64>(TickNum);
			}
			double SeqNum = 0;
			if ((*PayloadObj)->TryGetNumberField(TEXT("seq"), SeqNum))
			{
				Ack.Seq = static_cast<int64>(SeqNum);
			}
		}
		OnWorldAck.Broadcast(Ack);
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
