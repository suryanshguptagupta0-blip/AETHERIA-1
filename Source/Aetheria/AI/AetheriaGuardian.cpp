#include "AI/AetheriaGuardian.h"
#include "Engine/StaticMesh.h"
#include "Characters/AetheriaCharacter.h"
#include "Core/AetheriaPlayerState.h"
#include "Core/AetheriaPlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
namespace R = Aetheria::Rules;
AAetheriaGuardian::AAetheriaGuardian()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .05f;
    bReplicates = true; SetReplicateMovement(true); NetUpdateFrequency = 20;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); RootComponent = Collision;
    Collision->SetSphereRadius(105); Collision->SetCollisionProfileName(TEXT("Pawn"));
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(Collision);
    Warning = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackTelegraph")); Warning->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Sphere.Succeeded()) Body->SetStaticMesh(Sphere.Object); if (Cylinder.Succeeded()) Warning->SetStaticMesh(Cylinder.Object);
    Body->SetRelativeScale3D(FVector(2.1, 2.1, 3)); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Warning->SetRelativeLocation(FVector(0, 0, -100)); Warning->SetRelativeScale3D(FVector(6, 6, .025));
    Warning->SetCollisionEnabled(ECollisionEnabled::NoCollision); Warning->SetVisibility(false);
    RulesGuardian = R::CreateGuardian();
}
void AAetheriaGuardian::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(AAetheriaGuardian, Health); DOREPLIFETIME(AAetheriaGuardian, bTelegraph); DOREPLIFETIME(AAetheriaGuardian, bPhaseTwo); DOREPLIFETIME(AAetheriaGuardian, bAttackPhaseTwo);
}
void AAetheriaGuardian::OnRep_Telegraph() { Warning->SetVisibility(bTelegraph); Warning->SetRelativeScale3D(FVector(bAttackPhaseTwo ? 9.2 : 6.6, bAttackPhaseTwo ? 9.2 : 6.6, .025)); }
void AAetheriaGuardian::ReceiveCast(AAetheriaPlayerState* State, uint64 Id)
{
    if (!HasAuthority() || !State || !State->IsAlive()) return;
    const R::Result Result = R::ResolveGuardianHit(State->MutableRules(), RulesGuardian, Id, State->GetServerTimeMs());
    if (!Result.ok) return;
    Participants.AddUnique(State); Health = RulesGuardian.health; bPhaseTwo = RulesGuardian.phaseTwoReached;
    State->SyncPublicProfile(); State->SaveProfile();
    if (RulesGuardian.phase == R::GuardianPhase::Defeated)
    {
        bTelegraph = false; OnRep_Telegraph();
        for (const auto& Participant : Participants) if (AAetheriaPlayerState* Other = Participant.Get())
        {
            if (Other != State) R::ConfirmGuardianDefeat(Other->MutableRules(), RulesGuardian);
            Other->SyncPublicProfile(); Other->SaveProfile();
        }
    }
    ForceNetUpdate();
}
void AAetheriaGuardian::Attack()
{
    const float Range = bAttackPhaseTwo ? 460 : 330;
    for (TActorIterator<AAetheriaCharacter> It(GetWorld()); It; ++It)
    {
        AAetheriaPlayerState* State = It->GetPlayerState<AAetheriaPlayerState>();
        if (!State || !State->IsAlive() || State->GetRules().chapter != R::Chapter::GuardianEncounter || FVector::DistSquared(GetActorLocation(), It->GetActorLocation()) > FMath::Square(Range)) continue;
        FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(AetheriaGuardianAttack), false, this);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), It->GetActorLocation(), ECC_Visibility, Params);
        if (bBlocked && Hit.GetActor() != *It) continue;
        State->ApplyServerDamage(bAttackPhaseTwo ? 22 : 15);
        if (!State->IsAlive()) if (AAetheriaPlayerController* Controller = Cast<AAetheriaPlayerController>(It->GetController())) Controller->HandleKnockout();
    }
}
void AAetheriaGuardian::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (!HasAuthority()) return;
    if (SpawnPosition.IsNearlyZero()) SpawnPosition = GetActorLocation();
    APawn* Nearest = nullptr; double Distance = FMath::Square(2400.f);
    for (TActorIterator<AAetheriaCharacter> It(GetWorld()); It; ++It)
    {
        const AAetheriaPlayerState* State = It->GetPlayerState<AAetheriaPlayerState>();
        const double Current = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (State && State->IsAlive() && State->GetRules().chapter == R::Chapter::GuardianEncounter && Current < Distance) { Distance = Current; Nearest = *It; }
    }
    if (!Nearest || Health == 0)
    {
        NoTargetTime += DeltaSeconds;
        if (NoTargetTime > 20)
        {
            RulesGuardian = R::CreateGuardian(); Health = 240; bPhaseTwo = false; bAttackPhaseTwo = false; bTelegraph = false;
            Participants.Reset(); Target.Reset(); SetActorLocation(SpawnPosition); OnRep_Telegraph(); NoTargetTime = 0;
        }
        return;
    }
    NoTargetTime = 0; Target = Nearest;
    const double Now = GetWorld()->GetTimeSeconds();
    if (bTelegraph)
    {
        if (Now >= AttackAt) { Attack(); bTelegraph = false; OnRep_Telegraph(); RecoverUntil = Now + (bAttackPhaseTwo ? .7 : 1.2); }
        return;
    }
    if (Now < RecoverUntil) return;
    if (Distance < FMath::Square(bPhaseTwo ? 470.f : 340.f))
    { bAttackPhaseTwo = bPhaseTwo; bTelegraph = true; AttackAt = Now + (bAttackPhaseTwo ? .8 : 1.3); OnRep_Telegraph(); ForceNetUpdate(); return; }
    // Collision-aware engineering pursuit. Authored navmesh/BT validation is a later gate.
    FVector Direction = Nearest->GetActorLocation() - GetActorLocation(); Direction.Z = 0; Direction.Normalize();
    AddActorWorldOffset(Direction * DeltaSeconds * (bPhaseTwo ? 340 : 240), true);
    SetActorRotation(Direction.Rotation());
}
