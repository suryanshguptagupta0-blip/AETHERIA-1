#pragma once
#include "CoreMinimal.h"
#include "AetheriaUiTypes.generated.h"

USTRUCT(BlueprintType)
struct AETHERIA_API FAetheriaUiHudSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString PlayerName;
 UPROPERTY(BlueprintReadWrite) FString Location;
 UPROPERTY(BlueprintReadWrite) FString Objective;
 UPROPERTY(BlueprintReadWrite) FVector2D PlayerMapPosition = FVector2D::ZeroVector;
 UPROPERTY(BlueprintReadWrite) int32 Level = 1;
 UPROPERTY(BlueprintReadWrite) int64 Gold = 0;
 UPROPERTY(BlueprintReadWrite) int32 MasteryStage = 0;
 UPROPERTY(BlueprintReadWrite) float Health = 100.f;
 UPROPERTY(BlueprintReadWrite) float Mana = 100.f;
 UPROPERTY(BlueprintReadWrite) float Stamina = 100.f;
 UPROPERTY(BlueprintReadWrite) float Toxicity = 0.f;
 UPROPERTY(BlueprintReadWrite) FName ElementId = NAME_None;
 UPROPERTY(BlueprintReadWrite) FName NearbyNpcId = NAME_None;
 UPROPERTY(BlueprintReadWrite) FString NearbyNpcName;
 UPROPERTY(BlueprintReadWrite) bool bAwakened = false;
 UPROPERTY(BlueprintReadWrite) bool bCanInteract = false;
 UPROPERTY(BlueprintReadWrite) bool bCanTalk = false;
 UPROPERTY(BlueprintReadWrite) bool bCanCast = false;
};
USTRUCT(BlueprintType)
struct AETHERIA_API FAetheriaUiNpcMessage
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) int32 Sequence = 0;
 UPROPERTY(BlueprintReadWrite) FString Sender;
 UPROPERTY(BlueprintReadWrite) FString Text;
 UPROPERTY(BlueprintReadWrite) bool bFromPlayer = false;
};
USTRUCT(BlueprintType)
struct AETHERIA_API FAetheriaUiServiceAction
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FName Action = NAME_None;
 UPROPERTY(BlueprintReadWrite) FString Label;
 UPROPERTY(BlueprintReadWrite) int32 Amount = 0;
};
USTRUCT(BlueprintType)
struct AETHERIA_API FAetheriaUiNpcConversation
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FName NpcId = NAME_None;
 UPROPERTY(BlueprintReadWrite) FString Name;
 UPROPERTY(BlueprintReadWrite) FString Role;
 UPROPERTY(BlueprintReadWrite) FString Affinity;
 UPROPERTY(BlueprintReadWrite) FString Family;
 UPROPERTY(BlueprintReadWrite) int32 Familiarity = 0;
 UPROPERTY(BlueprintReadWrite) TArray<FAetheriaUiNpcMessage> Messages;
 UPROPERTY(BlueprintReadWrite) TArray<FString> Topics;
 UPROPERTY(BlueprintReadWrite) TArray<FAetheriaUiServiceAction> Services;
 UPROPERTY(BlueprintReadWrite) bool bAwaitingReply = false;
};
