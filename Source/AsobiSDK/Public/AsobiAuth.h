#pragma once

#include "CoreMinimal.h"
#include "AsobiClient.h"
#include "AsobiAuth.generated.h"

// On failure Tokens is empty and Error carries the HTTP status plus the
// backend's stable snake_case reason (`weak_device_secret`,
// `guest_capacity_reached`, `device_already_registered`, `guest_revoked`,
// `username_taken`, `validation_failed`, ...). Error.StatusCode is 0 when
// the request never got a response (Error.Reason == "network_error").
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAsobiAuthResponse, bool, bSuccess, const FAsobiAuthTokens&, Tokens, const FAsobiError&, Error);

struct FAsobiDeviceOptions;

UCLASS(BlueprintType)
class ASOBISDK_API UAsobiAuth : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Init(UAsobiClient* InClient);

	// POST /api/v1/auth/register
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Register(const FString& Username, const FString& Password, const FString& DisplayName, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/login
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Login(const FString& Username, const FString& Password, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/refresh
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Refresh(const FString& RefreshToken, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/oauth
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void OAuthAuthenticate(const FString& Provider, const FString& ProviderToken, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/guest (no auth header). Anonymous device-backed
	// create-or-resume. DeviceSecret is the caller-supplied base64 of >=32
	// CSPRNG bytes; store it securely and pass the same pair to resume.
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Guest(const FString& DeviceId, const FString& DeviceSecret, const FOnAsobiAuthResponse& Callback);

	// Opt-in convenience over Guest(): loads (or generates + persists) a stable
	// device credential pair and signs in as a guest in one call. The same pair
	// resumes the same player on a later launch. Uses a best-effort byte source
	// and a SaveGame-backed store; to supply a custom RNG or storage, call
	// AsobiDevice::LoadOrCreate yourself and pass the pair to Guest().
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void GuestDevice(const FOnAsobiAuthResponse& Callback);

	// C++-only variant of GuestDevice() that takes explicit device options
	// (custom RNG / storage slot / injected store). GuestDevice() forwards to
	// this with defaults. Not a UFUNCTION: FAsobiDeviceOptions is not a
	// Blueprint type.
	void GuestDeviceWithOptions(const FAsobiDeviceOptions& Options, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/guest/upgrade (authenticated). Claims the current
	// unclaimed guest with a username/password; replaces the stored token pair.
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void UpgradeGuest(const FString& Username, const FString& Password, const FOnAsobiAuthResponse& Callback);

	// POST /api/v1/auth/logout (authenticated). Sends the stored refresh token
	// so the backend revokes the whole family, then clears local tokens.
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void Logout(const FOnAsobiResponse& Callback);

	// POST /api/v1/auth/link (authenticated)
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void LinkProvider(const FString& Provider, const FString& ProviderToken, const FOnAsobiResponse& Callback);

	// DELETE /api/v1/auth/unlink (authenticated)
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void UnlinkProvider(const FString& Provider, const FOnAsobiResponse& Callback);

	// POST /api/v1/iap/apple (authenticated). Pass the StoreKit 2 JWS
	// signed transaction (the backend expects `signed_transaction`).
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void VerifyAppleIAP(const FString& SignedTransaction, const FOnAsobiResponse& Callback);

	// POST /api/v1/iap/google (authenticated)
	UFUNCTION(BlueprintCallable, Category = "Asobi|Auth")
	void VerifyGoogleIAP(const FString& PurchaseToken, const FString& ProductId, const FOnAsobiResponse& Callback);

private:
	void HandleAuthResponse(int32 StatusCode, const FString& ResponseBody, const FOnAsobiAuthResponse& Callback);

	UPROPERTY()
	UAsobiClient* Client = nullptr;
};
