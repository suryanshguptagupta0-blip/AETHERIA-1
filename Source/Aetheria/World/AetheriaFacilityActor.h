#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/AetheriaInteractable.h"
#include "AetheriaFacilityActor.generated.h"
class UBoxComponent;
UCLASS()
class AETHERIA_API AAetheriaFacilityActor : public AActor, public IAetheriaInteractable
{
    GENERATED_BODY()
public:
    AAetheriaFacilityActor();
    UPROPERTY(Replicated, EditAnywhere) FName FacilityId = TEXT("Hospital");
    bool Contains(FVector Position) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual FText GetInteractionLabel_Implementation() const override;
    virtual float GetInteractionRange_Implementation() const override { return 600; }
    virtual bool CanInteract_Implementation(APawn* Player) const override;
    virtual bool Interact_Implementation(APawn* Player) override;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Volume;
    UFUNCTION() void Enter(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
    bool Visit(APawn* Player);
};
