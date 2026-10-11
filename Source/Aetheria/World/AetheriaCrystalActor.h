#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/AetheriaInteractable.h"
#include "AetheriaCrystalActor.generated.h"
class UStaticMeshComponent;
UCLASS()
class AETHERIA_API AAetheriaCrystalActor : public AActor, public IAetheriaInteractable
{
    GENERATED_BODY()
public:
    AAetheriaCrystalActor();
    virtual FText GetInteractionLabel_Implementation() const override { return FText::FromString(TEXT("Touch the Awakening Crystal")); }
    virtual float GetInteractionRange_Implementation() const override { return 250; }
    virtual bool CanInteract_Implementation(APawn* Player) const override;
    virtual bool Interact_Implementation(APawn* Player) override;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Crystal;
};
