#include "AsobiAuth.h"
#include "AsobiDevice.h"
#include "AsobiCore/Auth.h"

void UAsobiAuth::Init(UAsobiClient* InClient)
{
	Client = InClient;
}

void UAsobiAuth::Register(const FString& Username, const FString& Password, const FString& DisplayName, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("username"), Username);
	Body->SetStringField(TEXT("password"), Password);
	if (!DisplayName.IsEmpty())
	{
		Body->SetStringField(TEXT("display_name"), DisplayName);
	}

	Client->PostWithStatus(TEXT("/api/v1/auth/register"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::Login(const FString& Username, const FString& Password, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("username"), Username);
	Body->SetStringField(TEXT("password"), Password);

	Client->PostWithStatus(TEXT("/api/v1/auth/login"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::Refresh(const FString& RefreshToken, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("refresh_token"), RefreshToken);

	Client->PostWithStatus(TEXT("/api/v1/auth/refresh"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::OAuthAuthenticate(const FString& Provider, const FString& ProviderToken, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("provider"), Provider);
	Body->SetStringField(TEXT("token"), ProviderToken);

	Client->PostWithStatus(TEXT("/api/v1/auth/oauth"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::Guest(const FString& DeviceId, const FString& DeviceSecret, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("device_id"), DeviceId);
	Body->SetStringField(TEXT("device_secret"), DeviceSecret);

	Client->PostWithStatus(TEXT("/api/v1/auth/guest"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::GuestDevice(const FOnAsobiAuthResponse& Callback)
{
	GuestDeviceWithOptions(FAsobiDeviceOptions(), Callback);
}

void UAsobiAuth::GuestDeviceWithOptions(const FAsobiDeviceOptions& Options, const FOnAsobiAuthResponse& Callback)
{
	const FAsobiDeviceCredentials Creds = AsobiDevice::LoadOrCreate(Options);
	Guest(Creds.DeviceId, Creds.DeviceSecret, Callback);
}

void UAsobiAuth::UpgradeGuest(const FString& Username, const FString& Password, const FOnAsobiAuthResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("username"), Username);
	Body->SetStringField(TEXT("password"), Password);

	Client->PostWithStatus(TEXT("/api/v1/auth/guest/upgrade"), Body,
		FOnAsobiStatusResponse::CreateLambda([this, Callback](bool, int32 StatusCode, const FString& Response)
		{
			HandleAuthResponse(StatusCode, Response, Callback);
		}));
}

void UAsobiAuth::Logout(const FOnAsobiResponse& Callback)
{
	if (!Client)
	{
		Callback.ExecuteIfBound(false, TEXT(""));
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("refresh_token"), Client->GetRefreshToken());

	TWeakObjectPtr<UAsobiClient> WeakClient(Client);
	Client->Post(TEXT("/api/v1/auth/logout"), Body,
		FOnAsobiResponse::CreateLambda([WeakClient, Callback](bool bSuccess, const FString& Response)
		{
			if (UAsobiClient* C = WeakClient.Get())
			{
				C->ClearTokens();
			}
			Callback.ExecuteIfBound(bSuccess, Response);
		}));
}

void UAsobiAuth::LinkProvider(const FString& Provider, const FString& ProviderToken, const FOnAsobiResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("provider"), Provider);
	Body->SetStringField(TEXT("token"), ProviderToken);

	Client->Post(TEXT("/api/v1/auth/link"), Body, Callback);
}

void UAsobiAuth::UnlinkProvider(const FString& Provider, const FOnAsobiResponse& Callback)
{
	Client->Delete(FString::Printf(TEXT("/api/v1/auth/unlink?provider=%s"), *Provider), Callback);
}

void UAsobiAuth::EraseAccount(const FString& Password, const FOnAsobiResponse& Callback)
{
	if (!Client)
	{
		Callback.ExecuteIfBound(false, TEXT(""));
		return;
	}

	// An empty password means "this account has none", so the field is left off
	// entirely rather than sent empty - an empty string would read as a
	// confirmation attempt that failed.
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	if (!Password.IsEmpty())
	{
		Body->SetStringField(TEXT("password"), Password);
	}

	TWeakObjectPtr<UAsobiClient> WeakClient(Client);
	Client->Post(TEXT("/api/v1/players/me/erase"), Body,
		FOnAsobiResponse::CreateLambda([WeakClient, Callback](bool bSuccess, const FString& Response)
		{
			// Only on success: a refusal leaves a live account, and signing the
			// player out of one they still have would be wrong.
			if (bSuccess)
			{
				if (UAsobiClient* C = WeakClient.Get())
				{
					C->ClearTokens();
				}
			}
			Callback.ExecuteIfBound(bSuccess, Response);
		}));
}

void UAsobiAuth::VerifyAppleIAP(const FString& SignedTransaction, const FOnAsobiResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("signed_transaction"), SignedTransaction);

	Client->Post(TEXT("/api/v1/iap/apple"), Body, Callback);
}

void UAsobiAuth::VerifyGoogleIAP(const FString& PurchaseToken, const FString& ProductId, const FOnAsobiResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("purchase_token"), PurchaseToken);
	Body->SetStringField(TEXT("product_id"), ProductId);

	Client->Post(TEXT("/api/v1/iap/google"), Body, Callback);
}

void UAsobiAuth::HandleAuthResponse(int32 StatusCode, const FString& ResponseBody, const FOnAsobiAuthResponse& Callback)
{
	const asobi::core::AuthResult Result =
		asobi::core::ParseAuthResponse(StatusCode, TCHAR_TO_UTF8(*ResponseBody));

	FAsobiAuthTokens Tokens;
	FAsobiError Error;

	if (Result.Success)
	{
		Tokens.AccessToken = UTF8_TO_TCHAR(Result.Tokens.AccessToken.c_str());
		Tokens.RefreshToken = UTF8_TO_TCHAR(Result.Tokens.RefreshToken.c_str());
		Tokens.PlayerId = UTF8_TO_TCHAR(Result.Tokens.PlayerId.c_str());
		Tokens.Username = UTF8_TO_TCHAR(Result.Tokens.Username.c_str());
		Tokens.bCreated = Result.Tokens.Created;
		Tokens.bGuest = Result.Tokens.Guest;
		Tokens.bUpgraded = Result.Tokens.Upgraded;

		// Auto-store tokens on the client
		if (Client)
		{
			Client->SetAuthToken(Tokens.AccessToken);
			Client->SetRefreshToken(Tokens.RefreshToken);
			Client->SetPlayerId(Tokens.PlayerId);
		}
	}
	else
	{
		Error.StatusCode = Result.Error.StatusCode;
		Error.Reason = UTF8_TO_TCHAR(Result.Error.Reason.c_str());
	}

	Callback.ExecuteIfBound(Result.Success, Tokens, Error);
}
