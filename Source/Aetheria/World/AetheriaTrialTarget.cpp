#include "World/AetheriaTrialTarget.h"
#include "Engine/StaticMesh.h"
#include "Core/AetheriaPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
AAetheriaTrialTarget::AAetheriaTrialTarget()
{
    bReplicates = true; Target = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialTarget")); RootComponent = Target;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Mesh.Succeeded()) Target->SetStaticMesh(Mesh.Object);
    Target->SetRelativeScale3D(FVector(1.3, 1.3, 2.2)); Target->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void AAetheriaTrialTarget::ReceiveCast(AAetheriaPlayerState* State, uint64 Id)
{
    if (!HasAuthority() || !State) return;
    const auto Event = bElementInteraction ? Aetheria::Rules::StoryEvent::TrialElementInteraction : Aetheria::Rules::StoryEvent::TrialTargetHit;
    const auto Result = Aetheria::Rules::ConsumeTrialCast(State->MutableRules(), Id, Event, State->GetServerTimeMs());
    if (Result.ok) { State->SyncPublicProfile(); State->SaveProfile(); }
}
