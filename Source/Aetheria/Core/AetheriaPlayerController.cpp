#include "Core/AetheriaPlayerController.h"
#include "Core/AetheriaPlayerState.h"
#include "Characters/AetheriaCharacter.h"
#include "World/AetheriaInteractable.h"
#include "World/AetheriaWorldRuntime.h"
#include "NPC/AetheriaNpcActor.h"
#include "UI/AetheriaRootWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace R = Aetheria::Rules;
namespace
{
struct FNpcMetadata { const TCHAR* Id; const TCHAR* Name; const TCHAR* Role; const TCHAR* Family; };
const FNpcMetadata Npcs[] = {
    {TEXT("DoctorMira"), TEXT("Dr. Mira Vale"), TEXT("Hospital physician"), TEXT("Lives with her sister and nephew in the hospital ward.")},
    {TEXT("ElderSeraphine"), TEXT("Elder Seraphine"), TEXT("Awakening mentor"), TEXT("Her family maintains the Awakening Hall archives.")},
    {TEXT("TutorElian"), TEXT("Tutor Elian"), TEXT("Academy tutor"), TEXT("Lives near the academy with his partner.")},
    {TEXT("RegistrarVera"), TEXT("Registrar Vera"), TEXT("Guild registrar"), TEXT("Her brother works in the market district.")},
    {TEXT("ResidentArden"), TEXT("Arden"), TEXT("Resident"), TEXT("Shares a home with his parents and daughter.")},
    {TEXT("MerchantNia"), TEXT("Nia"), TEXT("Market merchant"), TEXT("Runs a family shop with her mother.")},
    {TEXT("TellerOrin"), TEXT("Orin"), TEXT("Bank teller"), TEXT("Lives with his spouse in the city ward.")},
    {TEXT("InnkeeperSola"), TEXT("Sola"), TEXT("Innkeeper"), TEXT("Runs the inn with her cousin.")}
};
const FNpcMetadata* Metadata(FName Id) { for (const FNpcMetadata& Entry : Npcs) if (Id == FName(Entry.Id)) return &Entry; return nullptr; }
void AddService(FAetheriaUiNpcConversation& Conversation, const TCHAR* Action, const TCHAR* Label, int32 Amount = 0)
{
    FAetheriaUiServiceAction Service; Service.Action = FName(Action); Service.Label = Label; Service.Amount = Amount;
    Conversation.Services.Add(Service);
}
void PrepareConversation(FAetheriaUiNpcConversation& Conversation)
{
    if (const FNpcMetadata* Entry = Metadata(Conversation.NpcId))
    { Conversation.Name = Entry->Name; Conversation.Role = Entry->Role; Conversation.Family = Entry->Family; }
    Conversation.Affinity = TEXT("Ask about their work and magic.");
    Conversation.Topics = {TEXT("What should I do next?"), TEXT("Tell me about your family"), TEXT("What services do you offer?")};
    Conversation.Services.Reset();
    if (Conversation.NpcId == TEXT("DoctorMira")) AddService(Conversation, TEXT("Treat"), TEXT("Treatment • 20 crowns (emergency care free)"), 20);
    if (Conversation.NpcId == TEXT("TutorElian")) AddService(Conversation, TEXT("Train"), TEXT("Train affinity • 20 crowns, 8 mana, one trial credit"), 20);
    if (Conversation.NpcId == TEXT("MerchantNia")) AddService(Conversation, TEXT("BuyPotion"), TEXT("Buy mana potion • 25 crowns"), 25);
    if (Conversation.NpcId == TEXT("TellerOrin"))
    { AddService(Conversation, TEXT("Deposit"), TEXT("Deposit 100 crowns"), 100); AddService(Conversation, TEXT("Withdraw"), TEXT("Withdraw 100 crowns"), 100); }
    if (Conversation.NpcId == TEXT("InnkeeperSola")) AddService(Conversation, TEXT("Rest"), TEXT("Rest • 15 crowns"), 15);
}
}

AAetheriaPlayerController::AAetheriaPlayerController() { PrimaryActorTick.bCanEverTick = true; }
AAetheriaPlayerState* AAetheriaPlayerController::Profile() const { return GetPlayerState<AAetheriaPlayerState>(); }

void AAetheriaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    RootWidget = CreateWidget<UAetheriaRootWidget>(this, UAetheriaRootWidget::StaticClass());
    if (!RootWidget) return;
    RootWidget->AddToViewport();
    RootWidget->OnCreateCharacter.AddDynamic(this, &AAetheriaPlayerController::HandleCreateCharacter);
    RootWidget->OnSendNpcMessage.AddDynamic(this, &AAetheriaPlayerController::HandleNpcMessage);
    RootWidget->OnAction.AddDynamic(this, &AAetheriaPlayerController::HandleUiAction);
    RootWidget->OnMove.AddDynamic(this, &AAetheriaPlayerController::HandleMove);
    RootWidget->OnLook.AddDynamic(this, &AAetheriaPlayerController::HandleLook);
#if PLATFORM_ANDROID || PLATFORM_IOS
    RootWidget->SetInputMode(true);
#else
    RootWidget->SetInputMode(false);
#endif
    RootWidget->ShowMenu();
    SetUiEditing(true);
}

void AAetheriaPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalController() || !RootWidget) return;
    if (AAetheriaPlayerState* State = Profile())
    {
        const FAetheriaPublicProfile& Public = State->GetPublicProfile();
        if (Public.bIdentityCommitted && !bHadReplicatedIdentity)
        { bHadReplicatedIdentity = true; RootWidget->SetCharacterIdentity(Public.Name, Public.Avatar); }
    }
    HudAccumulator += DeltaSeconds;
    if (HudAccumulator >= .1) { HudAccumulator = 0; UpdateHud(); }
}

void AAetheriaPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (RootWidget) { RootWidget->RemoveFromParent(); RootWidget = nullptr; }
    Super::EndPlay(Reason);
}

void AAetheriaPlayerController::HandleCreateCharacter(const FString& Name, FName Avatar) { ServerCommitIdentity(Name, Avatar); }
void AAetheriaPlayerController::HandleNpcMessage(FName Id, const FString& Message) { ServerNpcMessage(Id, Message.Left(280)); }

void AAetheriaPlayerController::ServerCommitIdentity_Implementation(const FString& Name, FName Avatar)
{
    if (!Profile() || bJourneyEntered) return;
    if (!Profile()->CommitIdentity(Name, Avatar)) { ClientFeedback(TEXT("Character creation or save failed. Check name, avatar and server log.")); return; }
    ServerBeginJourney_Implementation();
}

void AAetheriaPlayerController::ServerBeginJourney_Implementation()
{
    if (!Profile() || !Profile()->GetPublicProfile().bIdentityCommitted) return;
    bJourneyEntered = true; bEditingUi = false;
    ClientBeginJourney();
}

void AAetheriaPlayerController::ClientBeginJourney_Implementation()
{
    bJourneyEntered = true; ActiveNpc = NAME_None;
    if (RootWidget) RootWidget->ShowHud();
    SetUiEditing(false);
}

void AAetheriaPlayerController::SetUiEditing(bool bEditing)
{
    bEditingUi = bEditing;
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    SetIgnoreMoveInput(bEditing); SetIgnoreLookInput(bEditing);
    if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn()))
    { Character->SprintReleased(); Character->SetTouchMove(FVector2D::ZeroVector); }
    if (!IsLocalController()) return;
    if (bEditing) { FInputModeUIOnly Mode; if (RootWidget) Mode.SetWidgetToFocus(RootWidget->TakeWidget()); SetInputMode(Mode); }
    else { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); }
    bShowMouseCursor = true;
    if (!HasAuthority()) ServerSetUiEditing(bEditing);
}

void AAetheriaPlayerController::ServerSetUiEditing_Implementation(bool bEditing)
{
    bEditingUi = bEditing;
    if (bEditing)
        if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn()))
        { Character->SprintReleased(); Character->GetCharacterMovement()->StopMovementImmediately(); }
}

bool AAetheriaPlayerController::IsNear(AActor* Target, float Range) const
{
    APawn* Pawn = GetPawn();
    if (!Pawn || !IsValid(Target) || !FMath::IsFinite(Range) || FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(FMath::Clamp(Range, 100.f, 650.f))) return false;
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(AetheriaInteraction), false, Pawn);
    const FVector Start = Pawn->GetActorLocation() + FVector(0, 0, 55);
    const FVector End = Target->GetActorLocation() + FVector(0, 0, 45);
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Target || (Hit.GetActor() && Hit.GetActor()->GetOwner() == Target);
}

AAetheriaNpcActor* AAetheriaPlayerController::FindNpc(FName Id) const
{
    if (!Metadata(Id)) return nullptr;
    for (TActorIterator<AAetheriaNpcActor> It(GetWorld()); It; ++It) if (It->GetNpcId() == Id) return *It;
    return nullptr;
}

void AAetheriaPlayerController::ServerInteract_Implementation(AActor* Target)
{
    if (!IsGameplayInputAllowed() || !Profile() || !Profile()->IsAlive() || !Target || !Target->GetClass()->ImplementsInterface(UAetheriaInteractable::StaticClass())) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastInteraction < .25 || !IsNear(Target, IAetheriaInteractable::Execute_GetInteractionRange(Target))) return;
    LastInteraction = Now;
    if (!IAetheriaInteractable::Execute_CanInteract(Target, GetPawn())) return;
    IAetheriaInteractable::Execute_Interact(Target, GetPawn());
    Profile()->SyncPublicProfile();
}

void AAetheriaPlayerController::ServerNpcMessage_Implementation(FName Id, const FString& Message)
{
    if (!bJourneyEntered || !Profile() || !Profile()->IsAlive()) return;
    const FString Clean = Message.TrimStartAndEnd();
    const double Now = GetWorld()->GetTimeSeconds();
    AAetheriaNpcActor* Npc = FindNpc(Id);
    if (!Npc || Clean.IsEmpty() || Clean.Len() > 280 || Now - LastNpcMessage < .35 || !IsNear(Npc, 425)) { ClientFeedback(TEXT("Move near the NPC and send a message up to 280 characters.")); return; }
    for (TCHAR C : Clean) if ((C < 32 && C != '\n' && C != '\t') || C == 127) return;
    LastNpcMessage = Now;
    const FText Reply = Npc->HandleConversation(GetPawn(), Clean);
    ReceiveNpcReply(Npc, Clean, Reply);
}

void AAetheriaPlayerController::ReceiveNpcReply(AAetheriaNpcActor* Npc, const FString& PlayerText, const FText& Reply)
{
    if (!HasAuthority() || !Npc || !Profile() || !IsNear(Npc, 425)) return;
    FAetheriaUiNpcConversation Conversation = Profile()->GetConversation(Npc->GetNpcId());
    PrepareConversation(Conversation);
    if (Conversation.NpcId == TEXT("ElderSeraphine") && Profile()->GetRules().chapter == R::Chapter::FinalDecision && Profile()->GetRules().mentorMet)
    { AddService(Conversation, TEXT("Seal"), TEXT("Seal the fracture")); AddService(Conversation, TEXT("Study"), TEXT("Preserve the fragment for study")); }
    const int32 Previous = Conversation.Messages.IsEmpty() ? 0 : Conversation.Messages.Last().Sequence;
    int32 Sequence = Previous >= MAX_int32 - 2 ? 0 : Previous;
    if (!PlayerText.IsEmpty())
    {
        FAetheriaUiNpcMessage Player; Player.Sequence = ++Sequence; Player.Sender = Profile()->GetPublicProfile().Name;
        Player.Text = PlayerText.Left(280); Player.bFromPlayer = true; Conversation.Messages.Add(Player);
        Conversation.Familiarity = FMath::Min(100, Conversation.Familiarity + 1);
    }
    FAetheriaUiNpcMessage Answer; Answer.Sequence = ++Sequence; Answer.Sender = Conversation.Name;
    Answer.Text = Reply.ToString().Left(280); Answer.bFromPlayer = false; Conversation.Messages.Add(Answer);
    Conversation.bAwaitingReply = false;
    Profile()->RecordConversation(Conversation); Profile()->SaveProfile();
    ClientConversation(Profile()->GetConversation(Npc->GetNpcId()));
}

void AAetheriaPlayerController::ClientConversation_Implementation(const FAetheriaUiNpcConversation& Conversation)
{
    ActiveNpc = Conversation.NpcId;
    if (RootWidget) { RootWidget->SetNpcConversation(Conversation); RootWidget->ShowConversation(); }
    SetUiEditing(true);
}

void AAetheriaPlayerController::ServerRequestService_Implementation(FName Id, FName Action)
{
    if (!bJourneyEntered || !Profile()) return;
    AAetheriaNpcActor* Npc = FindNpc(Id);
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Npc || !IsNear(Npc, 425) || Now - LastService < .65) return;
    LastService = Now;
    R::State& State = Profile()->MutableRules();
    R::Result Result; Result.message = "This NPC does not provide that service.";
    if (Id == TEXT("TellerOrin") && Action == TEXT("Deposit")) Result = R::Deposit(State, 100);
    else if (Id == TEXT("TellerOrin") && Action == TEXT("Withdraw")) Result = R::Withdraw(State, 100);
    else if (Id == TEXT("DoctorMira") && Action == TEXT("Treat")) Result = R::TreatAtHospital(State);
    else if (Id == TEXT("TutorElian") && Action == TEXT("Train")) Result = R::Train(State, State.affinity);
    else if (Id == TEXT("MerchantNia") && Action == TEXT("BuyPotion")) Result = R::BuyItem(State, "mana-potion", 1);
    else if (Id == TEXT("InnkeeperSola") && Action == TEXT("Rest")) Result = R::RestAtInn(State);
    Profile()->SyncPublicProfile(); if (Result.ok) Profile()->SaveProfile();
    ClientFeedback(UTF8_TO_TCHAR(Result.message.c_str()));
}

void AAetheriaPlayerController::ServerChooseFinal_Implementation(bool bSeal)
{
    if (!bJourneyEntered || !Profile() || !Profile()->IsAlive() || !IsNear(FindNpc(TEXT("ElderSeraphine")), 425)) return;
    const R::Result Result = R::ChooseFinal(Profile()->MutableRules(), bSeal ? R::FinalChoice::Seal : R::FinalChoice::Study);
    Profile()->SyncPublicProfile(); if (Result.ok) Profile()->SaveProfile();
    ClientFeedback(UTF8_TO_TCHAR(Result.message.c_str()));
}

void AAetheriaPlayerController::HandleKnockout()
{
    if (!HasAuthority() || !Profile() || Profile()->IsAlive()) return;
    AAetheriaWorldRuntime* District = Cast<AAetheriaWorldRuntime>(UGameplayStatics::GetActorOfClass(this, AAetheriaWorldRuntime::StaticClass()));
    APawn* Pawn = GetPawn(); if (!District || !Pawn) return;
    Pawn->TeleportTo(District->GetHospitalSpawn(), District->GetHospitalSpawnRotation());
    if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
    const R::Result Recovery = R::TreatAtHospital(Profile()->MutableRules());
    Profile()->SyncPublicProfile(); Profile()->SaveProfile();
    if (Recovery.ok) ClientFeedback(TEXT("Emergency stabilization complete at the hospital. Your chapter progress is saved."));
}

void AAetheriaPlayerController::ClientFeedback_Implementation(const FString& Message)
{
    PendingFeedback = Message.Left(280); FeedbackUntil = GetWorld()->GetTimeSeconds() + 5;
    if (RootWidget) RootWidget->ShowFeedback(PendingFeedback);
    UpdateHud();
}

void AAetheriaPlayerController::HandleMove(FVector2D Value)
{
    if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn())) Character->SetTouchMove(IsGameplayInputAllowed() ? Value : FVector2D::ZeroVector);
}
void AAetheriaPlayerController::HandleLook(FVector2D Value)
{
    if (IsGameplayInputAllowed()) if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn())) Character->SetTouchLook(Value);
}

void AAetheriaPlayerController::HandleUiAction(FName Action)
{
    AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn());
    if (Action == TEXT("SprintUp")) { if (Character) Character->SprintReleased(); return; }
    if (Action == TEXT("Play")) { ServerBeginJourney(); return; }
    if (Action == TEXT("CloseChat")) { ActiveNpc = NAME_None; if (RootWidget) RootWidget->ShowHud(); SetUiEditing(false); return; }
    if (Action == TEXT("Settings")) { if (RootWidget) RootWidget->ShowSettings(true); SetUiEditing(true); return; }
    if (Action == TEXT("Map")) { if (RootWidget) RootWidget->ShowMap(); SetUiEditing(true); return; }
    if (Action == TEXT("CloseMap")) { if (RootWidget) RootWidget->ShowHud(); SetUiEditing(false); return; }
    if (Action == TEXT("CloseSettings")) { if (RootWidget) RootWidget->ShowHud(); SetUiEditing(false); return; }
    if (Action.ToString().StartsWith(TEXT("Service_"))) { ServerRequestService(ActiveNpc, FName(*Action.ToString().RightChop(8))); return; }
    if (Action == TEXT("Seal")) { ServerChooseFinal(true); return; }
    if (Action == TEXT("Study")) { ServerChooseFinal(false); return; }
    if (Action.ToString().StartsWith(TEXT("Quality")))
    {
        if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
        { Settings->SetOverallScalabilityLevel(Action == TEXT("QualityHigh") ? 2 : 0); Settings->ApplySettings(false); Settings->SaveSettings(); }
        return;
    }
    if (!IsGameplayInputAllowed() || !Character) return;
    if (Action == TEXT("Jump")) Character->Jump();
    else if (Action == TEXT("Dodge")) Character->DodgePressed();
    else if (Action == TEXT("SprintDown")) Character->SprintPressed();
    else if (Action == TEXT("Interact") || Action == TEXT("Talk")) Character->PerformInteract();
    else if (Action == TEXT("Cast")) Character->CastRequested();
}

void AAetheriaPlayerController::UpdateHud()
{
    if (!RootWidget || !Profile()) return;
    if (GetWorld()->GetTimeSeconds() >= FeedbackUntil) RootWidget->ShowFeedback(FString());
    const FAetheriaPublicProfile& Public = Profile()->GetPublicProfile();
    FAetheriaUiHudSnapshot Snapshot;
    Snapshot.PlayerName = Public.Name; Snapshot.Level = Public.Level;
    Snapshot.Gold = Public.Crowns;
    Snapshot.Health = Public.Health; Snapshot.Mana = Public.Mana; Snapshot.Stamina = Public.Stamina;
    Snapshot.Toxicity = Public.Fatigue; Snapshot.MasteryStage = Public.MasteryStage;
    Snapshot.bAwakened = Public.Element >= 0; Snapshot.bCanCast = Snapshot.bAwakened && Public.Health > 0;
    const TCHAR* ElementIds[] = {TEXT("Fire"), TEXT("Water"), TEXT("Air"), TEXT("Earth"), TEXT("Space"), TEXT("Shadow"), TEXT("Dark"), TEXT("Light"), TEXT("Electric"), TEXT("Summon"), TEXT("Healing")};
    Snapshot.ElementId = Public.Element >= 0 && Public.Element < 11 ? FName(ElementIds[Public.Element]) : NAME_None;
    Snapshot.Location = TEXT("Aetheria • Starting district");
    if (APawn* Pawn = GetPawn()) Snapshot.PlayerMapPosition = FVector2D(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y);
    Snapshot.Objective = GetWorld()->GetTimeSeconds() < FeedbackUntil ? PendingFeedback : Public.Objective;
    if (AAetheriaCharacter* Character = Cast<AAetheriaCharacter>(GetPawn()))
        if (AActor* Focus = Character->GetFocusedInteractable())
        {
            Snapshot.bCanInteract = true;
            if (AAetheriaNpcActor* Npc = Cast<AAetheriaNpcActor>(Focus))
            { Snapshot.bCanTalk = true; Snapshot.NearbyNpcId = Npc->GetNpcId(); if (const FNpcMetadata* Entry = Metadata(Npc->GetNpcId())) Snapshot.NearbyNpcName = Entry->Name; }
        }
    RootWidget->SetHud(Snapshot);
}
