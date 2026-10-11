#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UI/AetheriaUiTypes.h"
#include "AetheriaSaveGame.generated.h"

UCLASS()
class AETHERIA_API UAetheriaSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 EnvelopeVersion = 1;
    UPROPERTY(SaveGame) FString PersistentPlayerId;
    UPROPERTY(SaveGame) TArray<uint8> RulesBytes;
    UPROPERTY(SaveGame) bool bIdentityCommitted = false;
    UPROPERTY(SaveGame) TArray<FAetheriaUiNpcConversation> Conversations;
};
