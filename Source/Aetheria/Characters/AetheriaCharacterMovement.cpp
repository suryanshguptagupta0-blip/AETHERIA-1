#include "Characters/AetheriaCharacterMovement.h"
#include "Core/AetheriaPlayerController.h"
#include "Core/AetheriaPlayerState.h"
#include "GameFramework/Character.h"
namespace {
class FAetheriaSavedMove : public FSavedMove_Character
{
public:
    using Super = FSavedMove_Character;
    bool bSprint = false;
    virtual void Clear() override { Super::Clear(); bSprint = false; }
    virtual uint8 GetCompressedFlags() const override { return Super::GetCompressedFlags() | (bSprint ? FLAG_Custom_0 : 0); }
    virtual bool CanCombineWith(const FSavedMovePtr& Move, ACharacter* Character, float Delta) const override
    { return bSprint == static_cast<const FAetheriaSavedMove*>(Move.Get())->bSprint && Super::CanCombineWith(Move, Character, Delta); }
    virtual void SetMoveFor(ACharacter* Character, float Delta, const FVector& Acceleration, FNetworkPredictionData_Client_Character& Data) override
    { Super::SetMoveFor(Character, Delta, Acceleration, Data); bSprint = CastChecked<UAetheriaCharacterMovement>(Character->GetCharacterMovement())->bWantsToSprint; }
    virtual void PrepMoveFor(ACharacter* Character) override
    { Super::PrepMoveFor(Character); CastChecked<UAetheriaCharacterMovement>(Character->GetCharacterMovement())->bWantsToSprint = bSprint; }
};
class FAetheriaPrediction : public FNetworkPredictionData_Client_Character
{
public:
    using Super = FNetworkPredictionData_Client_Character;
    explicit FAetheriaPrediction(const UCharacterMovementComponent& Movement) : Super(Movement) { MaxSmoothNetUpdateDist = 92; NoSmoothNetUpdateDist = 140; }
    virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FAetheriaSavedMove()); }
};
}
float UAetheriaCharacterMovement::GetMaxSpeed() const
{
    const AAetheriaPlayerController* Controller = CharacterOwner ? Cast<AAetheriaPlayerController>(CharacterOwner->GetController()) : nullptr;
    const AAetheriaPlayerState* State = CharacterOwner ? CharacterOwner->GetPlayerState<AAetheriaPlayerState>() : nullptr;
    if (Controller && !Controller->IsGameplayInputAllowed()) return 0;
    if (State && !State->IsAlive()) return 0;
    return bWantsToSprint && State && State->GetPublicProfile().Stamina >= 2 ? 680.f : Super::GetMaxSpeed();
}
void UAetheriaCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    const AAetheriaPlayerController* Controller = CharacterOwner ? Cast<AAetheriaPlayerController>(CharacterOwner->GetController()) : nullptr;
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) && Controller && Controller->IsGameplayInputAllowed();
}
FNetworkPredictionData_Client* UAetheriaCharacterMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData) const_cast<UAetheriaCharacterMovement*>(this)->ClientPredictionData = new FAetheriaPrediction(*this);
    return ClientPredictionData;
}
