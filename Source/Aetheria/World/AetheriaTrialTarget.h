#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AetheriaTrialTarget.generated.h"
class AAetheriaPlayerState; class UStaticMeshComponent;
UCLASS()
class AETHERIA_API AAetheriaTrialTarget : public AActor
{
    GENERATED_BODY()
public:
    AAetheriaTrialTarget();
    bool bElementInteraction = false;
    void ReceiveCast(AAetheriaPlayerState* State, uint64 CastId);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Target;
};
