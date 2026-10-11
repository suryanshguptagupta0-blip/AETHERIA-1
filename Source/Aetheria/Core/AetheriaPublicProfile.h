#pragma once
#include "CoreMinimal.h"
#include "AetheriaPublicProfile.generated.h"

USTRUCT(BlueprintType)
struct AETHERIA_API FAetheriaPublicProfile
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name = TEXT("Traveler");
    UPROPERTY(BlueprintReadOnly) FName Avatar = TEXT("female");
    UPROPERTY(BlueprintReadOnly) int32 Element = -1;
    UPROPERTY(BlueprintReadOnly) int32 MasteryStage = 0;
    UPROPERTY(BlueprintReadOnly) int32 Chapter = 0;
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) float Health = 100.f;
    UPROPERTY(BlueprintReadOnly) float Mana = 100.f;
    UPROPERTY(BlueprintReadOnly) float Stamina = 100.f;
    UPROPERTY(BlueprintReadOnly) float Fatigue = 0.f;
    UPROPERTY(BlueprintReadOnly) int64 Crowns = 0;
    UPROPERTY(BlueprintReadOnly) int64 BankCrowns = 0;
    UPROPERTY(BlueprintReadOnly) FString Objective;
    UPROPERTY(BlueprintReadOnly) int32 FinalChoice = 0;
    UPROPERTY(BlueprintReadOnly) bool bIdentityCommitted = false;
};
