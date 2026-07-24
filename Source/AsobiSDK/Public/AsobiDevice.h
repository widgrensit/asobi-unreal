#pragma once

#include "CoreMinimal.h"

// Opt-in guest device-credential helper.
//
// Guest sign-in needs a stable {DeviceId, DeviceSecret} that the game generates
// once, persists, and re-presents on every launch (the same pair resumes the
// same player). This helper does that dance so a game does not hand-roll base64
// + persistence + the >=32-byte rule. It is entirely optional: pass your own
// values to UAsobiAuth::Guest(...) if you want your own storage or key source.
//
// DeviceId:     any stable, per-install id (default: base64 of 16 random bytes).
// DeviceSecret: standard base64 (RFC 4648, '+/' alphabet, '=' padding) of >= 32
//               random bytes. The server requires this exact shape and rejects
//               anything shorter with `weak_device_secret`.

// A stable guest device credential pair.
struct FAsobiDeviceCredentials
{
	FString DeviceId;
	FString DeviceSecret;
};

// Pluggable persistence for the device pair. The default implementation is
// SaveGame-backed (UGameplayStatics); tests inject an in-memory implementation.
class ASOBISDK_API IAsobiDeviceStore
{
public:
	virtual ~IAsobiDeviceStore() = default;

	// Loads a persisted pair into OutCreds. Returns false if none is stored
	// or the stored pair is incomplete.
	virtual bool Load(FAsobiDeviceCredentials& OutCreds) = 0;

	// Persists the pair. Returns false on write failure.
	virtual bool Save(const FAsobiDeviceCredentials& Creds) = 0;

	// Erases any persisted pair.
	virtual void Clear() = 0;
};

// Options for the device helper. Every field is optional. This is a plain C++
// struct (not a USTRUCT) because RandomBytes and Store are not Blueprint types;
// they exist so tests can inject a deterministic byte source and an in-memory
// store, mirroring the Defold reference SDK.
struct FAsobiDeviceOptions
{
	// A stronger or deterministic byte source; must return exactly NumBytes.
	// Defaults to a best-effort generator (see AsobiDevice.cpp for the entropy
	// note) when left unset.
	TFunction<TArray<uint8>(int32 NumBytes)> RandomBytes;

	// Persistence override; defaults to a SaveGame-slot store.
	TSharedPtr<IAsobiDeviceStore> Store;

	// Fixed DeviceId override; else base64 of 16 random bytes.
	FString DeviceId;

	// SaveGame slot + user index used by the default store.
	FString SlotName = TEXT("AsobiGuestDevice");
	int32 UserIndex = 0;
};

namespace AsobiDevice
{
	// Generates a fresh {DeviceId, DeviceSecret} pair. Never persists.
	ASOBISDK_API FAsobiDeviceCredentials Generate(const FAsobiDeviceOptions& Options = FAsobiDeviceOptions());

	// Loads the persisted pair, or generates + persists one on first run.
	ASOBISDK_API FAsobiDeviceCredentials LoadOrCreate(const FAsobiDeviceOptions& Options = FAsobiDeviceOptions());

	// Erases the stored pair so the next LoadOrCreate / GuestDevice mints a
	// brand-new guest (Tokens.bCreated == true). Local-only: it does not delete
	// the server account. Pair it with Logout to end the current session, or
	// UpgradeGuest first if the player wants to keep the guest. Pass the same
	// SlotName/UserIndex you signed in with.
	ASOBISDK_API void Clear(const FAsobiDeviceOptions& Options = FAsobiDeviceOptions());
}
