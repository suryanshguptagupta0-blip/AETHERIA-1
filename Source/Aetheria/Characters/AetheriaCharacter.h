#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Engine/NetSerialization.h"
#include "AetheriaCharacter.generated.h"
class USpringArmComponent; class UCameraComponent; class UInputAction; class UInputMappingContext;
class UStaticMeshComponent;
class USkeletalMesh; class UAnimSequence;
UCLASS()
class AETHERIA_API AAetheriaCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    explicit AAetheriaCharacter(const FObjectInitializer& Initializer);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual bool CanJumpInternal_Implementation() const override;
    void PerformInteract(); void CastRequested(); void SprintPressed(); void SprintReleased(); void DodgePressed(); void TargetPressed(); void MapPressed();
    void SetTouchMove(FVector2D Value); void SetTouchLook(FVector2D Value);
    AActor* GetFocusedInteractable() const;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<USkeletalMesh> MaleMesh;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<USkeletalMesh> FemaleMesh;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> MaleIdle;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> MaleWalk;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> MaleRun;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> FemaleIdle;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> FemaleWalk;
    UPROPERTY(EditDefaultsOnly, Category="Avatar") TObjectPtr<UAnimSequence> FemaleRun;
    UFUNCTION(Server, Reliable) void ServerDodge();
    UFUNCTION(Server, Reliable) void ServerCastRequested(FName SpellId, FVector_NetQuantizeNormal Aim);
protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> EngineeringBody;
private:
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> LookAction;
    UPROPERTY() TObjectPtr<UInputAction> InteractAction;
    UPROPERTY() TObjectPtr<UInputAction> CastAction;
    UPROPERTY() TObjectPtr<UInputAction> DodgeAction;
    UPROPERTY() TObjectPtr<UInputAction> SprintAction;
    UPROPERTY() TObjectPtr<UInputAction> TargetAction;
    UPROPERTY() TObjectPtr<UInputAction> MapAction;
    FVector2D TouchMove = FVector2D::ZeroVector;
    TWeakObjectPtr<AActor> LockedTarget;
    double LastDodge = -10, LastCastIntent = -10, SprintDrain = 0;
    bool CanAct() const;
    void UpdateAvatar();
    FName VisibleAvatar = NAME_None;
    UPROPERTY() TObjectPtr<UAnimSequence> VisibleAnimation;
    void Move(const FInputActionValue& Value); void Look(const FInputActionValue& Value);
    void ApplyMove(FVector2D Value); void CreateMapping();
};
