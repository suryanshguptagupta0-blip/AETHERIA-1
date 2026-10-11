#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AetheriaCharacterMovement.generated.h"
UCLASS()
class AETHERIA_API UAetheriaCharacterMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    bool bWantsToSprint = false;
    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};
