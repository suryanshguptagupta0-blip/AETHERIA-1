#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AetheriaInteractable.generated.h"
class APawn;
UINTERFACE(BlueprintType)
class AETHERIA_API UAetheriaInteractable : public UInterface { GENERATED_BODY() };
class AETHERIA_API IAetheriaInteractable
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) FText GetInteractionLabel() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) float GetInteractionRange() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) bool CanInteract(APawn* Player) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) bool Interact(APawn* Player);
};
