#include "NPC/AetheriaNpcActor.h"
#include "Engine/StaticMesh.h"
#include "Core/AetheriaPlayerController.h"
#include "Core/AetheriaPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
namespace R = Aetheria::Rules;
AAetheriaNpcActor::AAetheriaNpcActor()
{
    bReplicates = true; Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EngineeringBody")); RootComponent = Body;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Mesh.Succeeded()) Body->SetStaticMesh(Mesh.Object);
    Body->SetWorldScale3D(FVector(.65, .65, 1.7)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void AAetheriaNpcActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const { Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(AAetheriaNpcActor, NpcId); }
FText AAetheriaNpcActor::GetInteractionLabel_Implementation() const { return FText::FromString(TEXT("Talk to ") + NpcId.ToString()); }
bool AAetheriaNpcActor::CanInteract_Implementation(APawn* Player) const
{
    const AAetheriaPlayerState* State = Player ? Player->GetPlayerState<AAetheriaPlayerState>() : nullptr;
    return State && State->GetPublicProfile().bIdentityCommitted && State->IsAlive();
}
bool AAetheriaNpcActor::Interact_Implementation(APawn* Player)
{
    if (!HasAuthority() || !CanInteract_Implementation(Player)) return false;
    AAetheriaPlayerState* State = Player->GetPlayerState<AAetheriaPlayerState>();
    if (NpcId == TEXT("DoctorMira") && State->GetRules().chapter == R::Chapter::HospitalStart) State->ApplyStoryEvent(R::StoryEvent::MeetDoctor);
    if (NpcId == TEXT("ElderSeraphine"))
    {
        if (State->GetRules().chapter == R::Chapter::AwakeningHall) State->ApplyStoryEvent(R::StoryEvent::MeetSeraphine);
        else if (State->GetRules().chapter == R::Chapter::FinalDecision) State->ApplyStoryEvent(R::StoryEvent::ReturnToSeraphine);
    }
    if (AAetheriaPlayerController* Controller = Cast<AAetheriaPlayerController>(Player->GetController())) Controller->ReceiveNpcReply(this, FString(), HandleConversation(Player, TEXT("hello")));
    return true;
}
FText AAetheriaNpcActor::HandleConversation(APawn* Player, const FString& Message)
{
    AAetheriaPlayerState* State = Player ? Player->GetPlayerState<AAetheriaPlayerState>() : nullptr;
    if (!HasAuthority() || !State) return FText::FromString(TEXT("Please speak to the server's resident."));
    const FString Lower = Message.ToLower(); FString Reply;
    if (Lower.Contains(TEXT("next")) || Lower.Contains(TEXT("quest")) || Lower.Contains(TEXT("help")) || Lower.Contains(TEXT("अगला")) || Lower.Contains(TEXT("मदद"))) Reply = UTF8_TO_TCHAR(R::GetObjective(State->GetRules()).c_str());
    else if (Lower.Contains(TEXT("family")) || Lower.Contains(TEXT("परिवार")))
    {
        if (NpcId == TEXT("DoctorMira")) Reply = TEXT("My sister and nephew live in the hospital ward. We eat together after my shift.");
        else if (NpcId == TEXT("ElderSeraphine")) Reply = TEXT("My family has cared for the Hall archives for generations. Each new awakening becomes part of our history.");
        else if (NpcId == TEXT("TutorElian")) Reply = TEXT("I share a home near the academy with my partner. We both teach young travelers.");
        else if (NpcId == TEXT("MerchantNia")) Reply = TEXT("My mother and I run this shop. She taught me to price honestly.");
        else if (NpcId == TEXT("TellerOrin")) Reply = TEXT("My spouse and I live in the city ward. We save for our own house.");
        else if (NpcId == TEXT("InnkeeperSola")) Reply = TEXT("My cousin helps run the inn. Guests bring stories from far beyond Aetheria.");
        else if (NpcId == TEXT("ResidentArden")) Reply = TEXT("My parents and daughter share my house. Safe streets matter to all of us.");
        else Reply = TEXT("My brother works in the market district. The guild and market keep our family busy.");
    }
    else if (Lower.Contains(TEXT("service")) || Lower.Contains(TEXT("buy")) || Lower.Contains(TEXT("bank")) || Lower == TEXT("train") || Lower.Contains(TEXT("training")))
    {
        if (NpcId == TEXT("DoctorMira")) Reply = TEXT("Use the treatment button. Emergency stabilization is free; later treatment costs 20 crowns.");
        else if (NpcId == TEXT("TutorElian")) Reply = TEXT("Training consumes a completed drill credit, 20 crowns and 8 mana. Use the explicit training button.");
        else if (NpcId == TEXT("TellerOrin")) Reply = TEXT("Deposit or withdraw 100 crowns with the service buttons. Your wallet and bank balance are conserved.");
        else if (NpcId == TEXT("MerchantNia")) Reply = TEXT("A mana potion costs 25 crowns. Buy with the service button after the city introduction opens.");
        else if (NpcId == TEXT("InnkeeperSola")) Reply = TEXT("Rest costs 15 crowns after the city introduction. The service button restores your resources.");
        else Reply = TEXT("I can explain your next step. Ask about the crystal, your objective or my family.");
    }
    else if (Lower.Contains(TEXT("magic")) || Lower.Contains(TEXT("crystal")) || Lower.Contains(TEXT("जादू"))) Reply = TEXT("Meet Seraphine and touch the crystal. The server awakens one random affinity at Basic. Two Advanced parents are required for fusion.");
    else if (Lower.Contains(TEXT("schedule")) || Lower.Contains(TEXT("routine")))
    {
        const int32 Hour = static_cast<int32>(FMath::Fmod(8.0 + GetWorld()->GetTimeSeconds() / 60.0, 24.0));
        Reply = Hour < 18 ? TEXT("This is my working shift. In the evening I meet my family for dinner.") : TEXT("Evening is family time. I will head home after helping my visitors.");
    }
    else
    {
        const bool bKnown = State->GetConversation(NpcId).Familiarity > 0;
        Reply = bKnown ? TEXT("Welcome back, ") + State->GetPublicProfile().Name + TEXT(". I remember our conversation. What would you like to know?") : TEXT("Welcome, ") + State->GetPublicProfile().Name + TEXT(". Ask about your next step, my family or the city.");
    }
    return FText::FromString(Reply.Left(280));
}
