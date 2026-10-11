#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/AetheriaInteractable.h"
#include "AetheriaNpcActor.generated.h"
class UStaticMeshComponent;
UCLASS()
class AETHERIA_API AAetheriaNpcActor : public AActor, public IAetheriaInteractable
{
    GENERATED_BODY()
public:
    AAetheriaNpcActor();
    UPROPERTY(Replicated, EditAnywhere) FName NpcId = TEXT("DoctorMira");
    FName GetNpcId() const { return NpcId; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual FText GetInteractionLabel_Implementation() const override;
    virtual float GetInteractionRange_Implementation() const override { return 425; }
    virtual bool CanInteract_Implementation(APawn* Player) const override;
    virtual bool Interact_Implementation(APawn* Player) override;
    FText HandleConversation(APawn* Player, const FString& Message);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
};
