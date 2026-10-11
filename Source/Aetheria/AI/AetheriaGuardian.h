#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Rules/AetheriaRules.h"
#include "AetheriaGuardian.generated.h"
class USphereComponent; class UStaticMeshComponent; class AAetheriaPlayerState;
UCLASS()
class AETHERIA_API AAetheriaGuardian : public APawn
{
    GENERATED_BODY()
public:
    AAetheriaGuardian();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void ReceiveCast(AAetheriaPlayerState* State, uint64 CastId);
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Health = 240;
    UPROPERTY(ReplicatedUsing=OnRep_Telegraph, BlueprintReadOnly) bool bTelegraph = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bPhaseTwo = false;
private:
    // Capture the phase when a windup starts so its warning and hit stay aligned.
    UPROPERTY(ReplicatedUsing=OnRep_Telegraph) bool bAttackPhaseTwo = false;
    UPROPERTY() TObjectPtr<USphereComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Warning;
    Aetheria::Rules::GuardianState RulesGuardian;
    TArray<TWeakObjectPtr<AAetheriaPlayerState>> Participants;
    TWeakObjectPtr<APawn> Target;
    FVector SpawnPosition;
    double AttackAt = 0, RecoverUntil = 0, NoTargetTime = 0;
    UFUNCTION() void OnRep_Telegraph();
    void Attack();
};
