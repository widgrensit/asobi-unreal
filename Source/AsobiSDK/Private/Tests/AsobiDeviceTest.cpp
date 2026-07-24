// Unit tests for the opt-in guest device-credential helper (AsobiDevice) and
// the UAsobiAuth::GuestDevice convenience.
//
// These are synchronous UE Automation tests - no backend, no HTTP. Persistence
// and the byte source are injected via FAsobiDeviceOptions (an in-memory store
// and a deterministic RandomBytes), so the helper is exercised without touching
// the SaveGame subsystem or a CSPRNG. Mirrors asobi-defold/tests/test_device.lua.
//
// Run (needs an installed Unreal Engine - not available on stock CI):
//
//   UE5Editor-Cmd /path/to/MyProject.uproject \
//     -ExecCmds="Automation RunTests Asobi.Device; Quit" \
//     -unattended -nullrhi -log
//
// The engine-agnostic dispatch gate runs on stock CI via AsobiCore/doctest;
// this device helper depends on FBase64 + UGameplayStatics + USaveGame and so
// can only run under the UE toolchain.

#include "AsobiAuth.h"
#include "AsobiClient.h"
#include "AsobiDevice.h"

#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AsobiDeviceTestDetail
{
	// In-memory store so persistence is exercised without the SaveGame slot.
	class FMemoryDeviceStore : public IAsobiDeviceStore
	{
	public:
		bool bHas = false;
		FAsobiDeviceCredentials Stored;

		virtual bool Load(FAsobiDeviceCredentials& OutCreds) override
		{
			if (!bHas)
			{
				return false;
			}
			OutCreds = Stored;
			return true;
		}

		virtual bool Save(const FAsobiDeviceCredentials& Creds) override
		{
			Stored = Creds;
			bHas = true;
			return true;
		}

		virtual void Clear() override
		{
			bHas = false;
			Stored = FAsobiDeviceCredentials();
		}
	};

	// Decodes standard base64; returns the byte count or -1 if malformed.
	static int32 DecodedByteCount(const FString& Encoded)
	{
		TArray<uint8> Out;
		return FBase64::Decode(Encoded, Out) ? Out.Num() : -1;
	}

	// A deterministic byte source: N distinct bytes, so id and secret differ.
	static TArray<uint8> CounterBytes(int32 NumBytes)
	{
		TArray<uint8> Bytes;
		Bytes.SetNum(NumBytes);
		for (int32 i = 0; i < NumBytes; ++i)
		{
			Bytes[i] = static_cast<uint8>((i * 7 + 1) & 0xFF);
		}
		return Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsobiDeviceTest,
	"Asobi.Device",
	EAutomationTestFlags::ProductFilter
	| EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::EditorContext)

bool FAsobiDeviceTest::RunTest(const FString& Parameters)
{
	using namespace AsobiDeviceTestDetail;

	// --- Generate: well-formed, standard-base64, 32-byte secret ------------
	{
		const FAsobiDeviceCredentials Creds = AsobiDevice::Generate();
		TestFalse(TEXT("device_id is non-empty"), Creds.DeviceId.IsEmpty());
		TestFalse(TEXT("device_secret is non-empty"), Creds.DeviceSecret.IsEmpty());
		TestEqual(TEXT("device_secret decodes to exactly 32 bytes"),
			DecodedByteCount(Creds.DeviceSecret), 32);
		TestEqual(TEXT("device_id decodes to 16 bytes"),
			DecodedByteCount(Creds.DeviceId), 16);
		// Standard alphabet only - never the url-safe '-' or '_'.
		TestFalse(TEXT("secret uses the standard base64 alphabet"),
			Creds.DeviceSecret.Contains(TEXT("-")) || Creds.DeviceSecret.Contains(TEXT("_")));
	}

	// --- Generate: two calls yield distinct pairs --------------------------
	{
		const FAsobiDeviceCredentials A = AsobiDevice::Generate();
		const FAsobiDeviceCredentials B = AsobiDevice::Generate();
		TestNotEqual(TEXT("distinct device_ids across generations"), A.DeviceId, B.DeviceId);
		TestNotEqual(TEXT("distinct device_secrets across generations"), A.DeviceSecret, B.DeviceSecret);
	}

	// --- Generate: custom RandomBytes flows through unchanged ---------------
	{
		FAsobiDeviceOptions Options;
		Options.RandomBytes = [](int32 NumBytes)
		{
			TArray<uint8> Bytes;
			Bytes.Init(0x41, NumBytes); // all 'A'
			return Bytes;
		};
		const FAsobiDeviceCredentials Creds = AsobiDevice::Generate(Options);
		TestEqual(TEXT("custom-sourced secret still decodes to 32 bytes"),
			DecodedByteCount(Creds.DeviceSecret), 32);
		TArray<uint8> Decoded;
		FBase64::Decode(Creds.DeviceSecret, Decoded);
		bool bAllA = Decoded.Num() == 32;
		for (uint8 B : Decoded)
		{
			bAllA = bAllA && (B == 0x41);
		}
		TestTrue(TEXT("custom bytes flow through unchanged (all 0x41)"), bAllA);
	}

	// --- Generate: explicit DeviceId used verbatim -------------------------
	{
		FAsobiDeviceOptions Options;
		Options.DeviceId = TEXT("my-stable-id");
		const FAsobiDeviceCredentials Creds = AsobiDevice::Generate(Options);
		TestEqual(TEXT("explicit device_id used verbatim"), Creds.DeviceId, FString(TEXT("my-stable-id")));
		TestEqual(TEXT("secret still well-formed with explicit id"),
			DecodedByteCount(Creds.DeviceSecret), 32);
	}

	// --- LoadOrCreate: persists then resumes the same pair -----------------
	{
		TSharedPtr<FMemoryDeviceStore> Store = MakeShared<FMemoryDeviceStore>();
		FAsobiDeviceOptions Options;
		Options.Store = Store;
		Options.RandomBytes = &CounterBytes;

		const FAsobiDeviceCredentials First = AsobiDevice::LoadOrCreate(Options);
		TestTrue(TEXT("first LoadOrCreate persisted a pair"), Store->bHas);

		const FAsobiDeviceCredentials Second = AsobiDevice::LoadOrCreate(Options);
		TestEqual(TEXT("second LoadOrCreate resumes the same device_id"), Second.DeviceId, First.DeviceId);
		TestEqual(TEXT("second LoadOrCreate resumes the same device_secret"), Second.DeviceSecret, First.DeviceSecret);
	}

	// --- LoadOrCreate: regenerates when the stored pair is invalid ----------
	{
		TSharedPtr<FMemoryDeviceStore> Store = MakeShared<FMemoryDeviceStore>();
		Store->bHas = true;
		Store->Stored.DeviceId = TEXT("only-id"); // missing secret -> Load() returns false
		FAsobiDeviceOptions Options;
		Options.Store = Store;

		const FAsobiDeviceCredentials Creds = AsobiDevice::LoadOrCreate(Options);
		TestEqual(TEXT("regenerated a well-formed secret when the stored pair was invalid"),
			DecodedByteCount(Creds.DeviceSecret), 32);
	}

	// --- Clear: erases -> next LoadOrCreate mints a fresh guest -------------
	{
		TSharedPtr<FMemoryDeviceStore> Store = MakeShared<FMemoryDeviceStore>();
		FAsobiDeviceOptions Options;
		Options.Store = Store;

		const FAsobiDeviceCredentials First = AsobiDevice::LoadOrCreate(Options);
		AsobiDevice::Clear(Options);
		TestFalse(TEXT("cleared pair gone from storage"), Store->bHas);

		const FAsobiDeviceCredentials Second = AsobiDevice::LoadOrCreate(Options);
		TestNotEqual(TEXT("a new guest id is minted after clear"), Second.DeviceId, First.DeviceId);
	}

	// --- GuestDevice: resolves + persists the creds it forwards to Guest ----
	// The Guest() HTTP call itself needs a backend (covered by the smoke flow);
	// here we assert the forwarding seam - GuestDeviceWithOptions loads/persists
	// a well-formed pair through the injected store before dispatching, and a
	// second call reuses that exact pair.
	{
		TSharedPtr<FMemoryDeviceStore> Store = MakeShared<FMemoryDeviceStore>();
		FAsobiDeviceOptions Options;
		Options.Store = Store;

		UAsobiClient* Client = NewObject<UAsobiClient>();
		Client->SetBaseUrl(TEXT("http://127.0.0.1:1")); // unreachable; HTTP fires async, ignored
		UAsobiAuth* Auth = NewObject<UAsobiAuth>();
		Auth->Init(Client);

		// The dispatch fires an async HTTP whose lambda captures Client/Auth. Root
		// both so a GC between here and the callback cannot free them mid-flight;
		// unroot at scope exit once the assertions are done.
		Client->AddToRoot();
		Auth->AddToRoot();

		Auth->GuestDeviceWithOptions(Options, FOnAsobiAuthResponse());
		TestTrue(TEXT("GuestDevice persisted a pair before forwarding"), Store->bHas);
		TestEqual(TEXT("forwarded a well-formed 32-byte secret"),
			DecodedByteCount(Store->Stored.DeviceSecret), 32);

		const FAsobiDeviceCredentials Forwarded = Store->Stored;
		Auth->GuestDeviceWithOptions(Options, FOnAsobiAuthResponse());
		TestEqual(TEXT("second GuestDevice reuses the persisted device_id"), Store->Stored.DeviceId, Forwarded.DeviceId);
		TestEqual(TEXT("second GuestDevice reuses the persisted device_secret"), Store->Stored.DeviceSecret, Forwarded.DeviceSecret);

		Auth->RemoveFromRoot();
		Client->RemoveFromRoot();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
