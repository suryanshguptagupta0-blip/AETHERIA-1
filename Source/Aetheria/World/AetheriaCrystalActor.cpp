#include "World/AetheriaCrystalActor.h"
#include "Engine/StaticMesh.h"
#include "Core/AetheriaPlayerState.h"
#include "Core/AetheriaPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
AAetheriaCrystalActor::AAetheriaCrystalActor()
{
    bReplicates = true; Crystal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crystal")); RootComponent = Crystal;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (Mesh.Succeeded()) Crystal->SetStaticMesh(Mesh.Object);
    Crystal->SetRelativeScale3D(FVector(1.1, 1.1, 2.3)); Crystal->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
bool AAetheriaCrystalActor::CanInteract_Implementation(APawn* Player) const
{
    const AAetheriaPlayerState* State = Player ? Player->GetPlayerState<AAetheriaPlayerState>() : nullptr;
    return State && State->IsAlive() && State->GetPublicProfile().Chapter == static_cast<int32>(Aetheria::Rules::Chapter::AwakeningHall);
}
bool AAetheriaCrystalActor::Interact_Implementation(APawn* Player)
{
    if (!HasAuthority() || !CanInteract_Implementation(Player)) return false;
    AAetheriaPlayerState* State = Player->GetPlayerState<AAetheriaPlayerState>(); const bool bSuccess = State->RequestAwakening();
    if (AAetheriaPlayerController* Controller = Cast<AAetheriaPlayerController>(Player->GetController())) Controller->ClientFeedback(bSuccess ? TEXT("Your affinity awakened at Basic. Walk to the magic trial.") : TEXT("Meet Elder Seraphine before touching the crystal."));
    return bSuccess;
}
