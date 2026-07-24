#include "AsobiDevice.h"
#include "AsobiSaveGame.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"

namespace
{
	// UE ships no portable CSPRNG in its public API. This default mixes several
	// entropy sources - a platform GUID (OS-provided: CoCreateGuid on Windows,
	// CFUUID on Apple, a time/pid/cycle/rand mix elsewhere), the high-resolution
	// cycle counter, and a spreading multiply - into the requested bytes. It is
	// BEST-EFFORT and NOT a guaranteed CSPRNG on every platform. For a persisted
	// guest credential it is adequate; for higher assurance pass
	// FAsobiDeviceOptions::RandomBytes backed by a real platform CSPRNG.
	TArray<uint8> DefaultRandomBytes(int32 NumBytes)
	{
		TArray<uint8> Bytes;
		if (NumBytes <= 0)
		{
			return Bytes;
		}
		Bytes.SetNumUninitialized(NumBytes);

		int32 Filled = 0;
		while (Filled < NumBytes)
		{
			const FGuid Guid = FGuid::NewGuid();
			const uint32 Words[4] = { Guid.A, Guid.B, Guid.C, Guid.D };
			const uint32 Mix = static_cast<uint32>(FPlatformTime::Cycles64());
			for (int32 WordIndex = 0; WordIndex < 4 && Filled < NumBytes; ++WordIndex)
			{
				const uint32 Word = Words[WordIndex] ^ (Mix * 2654435761u + static_cast<uint32>(Filled));
				for (int32 ByteIndex = 0; ByteIndex < 4 && Filled < NumBytes; ++ByteIndex)
				{
					Bytes[Filled++] = static_cast<uint8>((Word >> (ByteIndex * 8)) & 0xFF);
				}
			}
		}
		return Bytes;
	}

	// Default SaveGame-backed store. Persists the pair on UAsobiSaveGame in its
	// own slot, distinct from the session (refresh-token) slot.
	class FSaveGameDeviceStore : public IAsobiDeviceStore
	{
	public:
		FSaveGameDeviceStore(const FString& InSlot, int32 InUserIndex)
			: Slot(InSlot), UserIndex(InUserIndex)
		{
		}

		virtual bool Load(FAsobiDeviceCredentials& OutCreds) override
		{
			if (!UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
			{
				return false;
			}
			UAsobiSaveGame* Save = Cast<UAsobiSaveGame>(
				UGameplayStatics::LoadGameFromSlot(Slot, UserIndex));
			if (!Save || Save->DeviceId.IsEmpty() || Save->DeviceSecret.IsEmpty())
			{
				return false;
			}
			OutCreds.DeviceId = Save->DeviceId;
			OutCreds.DeviceSecret = Save->DeviceSecret;
			return true;
		}

		virtual bool Save(const FAsobiDeviceCredentials& Creds) override
		{
			UAsobiSaveGame* SaveObj = Cast<UAsobiSaveGame>(
				UGameplayStatics::CreateSaveGameObject(UAsobiSaveGame::StaticClass()));
			if (!SaveObj)
			{
				return false;
			}
			SaveObj->DeviceId = Creds.DeviceId;
			SaveObj->DeviceSecret = Creds.DeviceSecret;
			return UGameplayStatics::SaveGameToSlot(SaveObj, Slot, UserIndex);
		}

		virtual void Clear() override
		{
			if (UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
			{
				UGameplayStatics::DeleteGameInSlot(Slot, UserIndex);
			}
		}

	private:
		FString Slot;
		int32 UserIndex;
	};

	TArray<uint8> RandBytes(const FAsobiDeviceOptions& Options, int32 NumBytes)
	{
		return Options.RandomBytes ? Options.RandomBytes(NumBytes) : DefaultRandomBytes(NumBytes);
	}

	TSharedRef<IAsobiDeviceStore> ResolveStore(const FAsobiDeviceOptions& Options)
	{
		if (Options.Store.IsValid())
		{
			return Options.Store.ToSharedRef();
		}
		return MakeShared<FSaveGameDeviceStore>(Options.SlotName, Options.UserIndex);
	}
}

FAsobiDeviceCredentials AsobiDevice::Generate(const FAsobiDeviceOptions& Options)
{
	TArray<uint8> SecretBytes = RandBytes(Options, 32);
	// Fail loud here rather than as an opaque server `weak_device_secret` 4xx if
	// a custom byte source under-delivers: the secret must decode to >= 32 bytes.
	// checkf() is compiled out in Shipping/Test (DO_CHECK==0), so guard at runtime
	// too - otherwise SetNum(32) would zero-pad a short buffer into a weak secret.
	if (SecretBytes.Num() < 32)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[asobi] RandomBytes(32) returned %d bytes; refusing to mint a zero-padded weak device secret"),
			SecretBytes.Num());
		return FAsobiDeviceCredentials();
	}
	SecretBytes.SetNum(32);

	FAsobiDeviceCredentials Creds;
	// FBase64::Encode emits standard base64 (RFC 4648, '+/' with '=' padding),
	// which is exactly what the server's decoder expects.
	Creds.DeviceSecret = FBase64::Encode(SecretBytes);
	Creds.DeviceId = Options.DeviceId.IsEmpty()
		? FBase64::Encode(RandBytes(Options, 16))
		: Options.DeviceId;
	return Creds;
}

FAsobiDeviceCredentials AsobiDevice::LoadOrCreate(const FAsobiDeviceOptions& Options)
{
	TSharedRef<IAsobiDeviceStore> Store = ResolveStore(Options);

	FAsobiDeviceCredentials Creds;
	if (Store->Load(Creds))
	{
		return Creds;
	}

	Creds = Generate(Options);
	// Best-effort: if the save fails the caller still gets a usable pair for
	// this session, but a lost write means a different guest next launch.
	if (!Store->Save(Creds))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[asobi] could not persist guest device credentials; a new guest may be created next launch"));
	}
	return Creds;
}

void AsobiDevice::Clear(const FAsobiDeviceOptions& Options)
{
	ResolveStore(Options)->Clear();
}
