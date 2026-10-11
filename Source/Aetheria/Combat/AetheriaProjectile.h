#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/AetheriaRules.h"
#include "AetheriaProjectile.generated.h"
class USphereComponent; class UStaticMeshComponent; class UProjectileMovementComponent;
UCLASS()
class AETHERIA_API AAetheriaProjectile : public AActor
{
    GENERATED_BODY()
public:
    AAetheriaProjectile();
    void InitializeServer(APawn* Caster, const Aetheria::Rules::Result& Approved, const Aetheria::Rules::SpellSpec& Spec, FVector Direction);
    virtual void BeginPlay() override;
private:
    UPROPERTY() TObjectPtr<USphereComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY() TObjectPtr<UProjectileMovementComponent> Movement;
    uint64 CastId = 0;
    bool bResolved = false;
    UFUNCTION() void Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Result);
};
