#pragma once

#include "CoreMinimal.h"
#include "AsobiClient.h"
#include "AsobiTypes.h"
#include "AsobiWorld.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAsobiWorldListResponse, bool, bSuccess, const TArray<FAsobiWorldInfo>&, Worlds);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAsobiWorldResponse, bool, bSuccess, const FAsobiWorldInfo&, World);

UCLASS(BlueprintType)
class ASOBISDK_API UAsobiWorld : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Asobi")
	void Init(UAsobiClient* InClient);

	UFUNCTION(BlueprintCallable, Category = "Asobi|World")
	void List(const FString& Mode, bool bOnlyWithCapacity, const FOnAsobiWorldListResponse& Callback);

	UFUNCTION(BlueprintCallable, Category = "Asobi|World")
	void Get(const FString& WorldId, const FOnAsobiWorldResponse& Callback);

	UFUNCTION(BlueprintCallable, Category = "Asobi|World")
	void Create(const FString& Mode, const FOnAsobiWorldResponse& Callback);

private:
	UPROPERTY()
	UAsobiClient* Client;
};
