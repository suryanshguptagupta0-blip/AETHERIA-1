#include "World/AetheriaFacilityActor.h"
#include "Core/AetheriaPlayerState.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
namespace R = Aetheria::Rules;
AAetheriaFacilityActor::AAetheriaFacilityActor()
{
    bReplicates = true; Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("FacilityVolume")); RootComponent = Volume;
    Volume->SetBoxExtent(FVector(720, 620, 230)); Volume->SetCollisionProfileName(TEXT("OverlapOnlyPawn")); Volume->SetGenerateOverlapEvents(true);
    Volume->OnComponentBeginOverlap.AddDynamic(this, &AAetheriaFacilityActor::Enter);
}
void AAetheriaFacilityActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const { Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(AAetheriaFacilityActor, FacilityId); }
bool AAetheriaFacilityActor::Contains(FVector Position) const { const FVector Local = GetActorTransform().InverseTransformPosition(Position); const FVector Extent = Volume->GetUnscaledBoxExtent(); return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z; }
FText AAetheriaFacilityActor::GetInteractionLabel_Implementation() const { return FText::FromString(TEXT("Visit ") + FacilityId.ToString()); }
bool AAetheriaFacilityActor::CanInteract_Implementation(APawn* Player) const { return Player && Contains(Player->GetActorLocation()); }
bool AAetheriaFacilityActor::Visit(APawn* Player)
{
    if (!HasAuthority() || !CanInteract_Implementation(Player)) return false;
    AAetheriaPlayerState* State = Player->GetPlayerState<AAetheriaPlayerState>(); if (!State || !State->IsAlive()) return false;
    const FName Ids[] = {TEXT("Hospital"), TEXT("Academy"), TEXT("Guild"), TEXT("Houses"), TEXT("Market"), TEXT("Bank"), TEXT("Inn")};
    for (int32 Index = 0; Index < 7; ++Index) if (FacilityId == Ids[Index]) return State->ApplyStoryEvent(static_cast<R::StoryEvent>(static_cast<uint8>(R::StoryEvent::VisitHospital) + Index));
    return false;
}
void AAetheriaFacilityActor::Enter(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit) { Visit(Cast<APawn>(Other)); }
bool AAetheriaFacilityActor::Interact_Implementation(APawn* Player) { return Visit(Player); }
