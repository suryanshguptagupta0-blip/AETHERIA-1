#include "Core/AetheriaPlayerState.h"
#include "Persistence/AetheriaSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include <limits>

namespace R = Aetheria::Rules;

AAetheriaPlayerState::AAetheriaPlayerState()
{
    PrimaryActorTick.bCanEverTick = true;
    RulesState = R::CreateNewPlayer("Traveler");
    NetUpdateFrequency = 5.f;
}

void AAetheriaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AAetheriaPlayerState, PublicProfile);
    DOREPLIFETIME_CONDITION(AAetheriaPlayerState, PersistentPlayerId, COND_OwnerOnly);
}

R::State& AAetheriaPlayerState::MutableRules()
{
    check(HasAuthority());
    bDirty = true;
    return RulesState;
}

void AAetheriaPlayerState::InitializeProfile(const FString& Id)
{
    if (!HasAuthority()) return;
    FGuid Guid;
    if (!FGuid::Parse(Id, Guid) || !Guid.IsValid()) return;
    PersistentPlayerId = Guid.ToString(EGuidFormats::DigitsWithHyphens);
    RulesState = R::CreateNewPlayer("Traveler");
    PublicProfile.bIdentityCommitted = false;
    const bool bExists = UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0);
    if (const UAetheriaSaveGame* Saved = Cast<UAetheriaSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0)))
    {
        std::vector<std::uint8_t> Bytes;
        Bytes.reserve(Saved->RulesBytes.Num());
        for (uint8 Byte : Saved->RulesBytes) Bytes.push_back(Byte);
        std::string Error;
        if (Saved->EnvelopeVersion == 1 && Saved->PersistentPlayerId == PersistentPlayerId && R::DecodeSave(Bytes, RulesState, &Error))
        {
            PublicProfile.bIdentityCommitted = Saved->bIdentityCommitted;
            for (const FAetheriaUiNpcConversation& Conversation : Saved->Conversations) RecordConversation(Conversation);
        }
        else
        {
            bPersistenceHealthy = false;
            UE_LOG(LogTemp, Error, TEXT("Rejected native profile %s: %s. Original slot preserved."), *PersistentPlayerId, UTF8_TO_TCHAR(Error.c_str()));
        }
    }
    else if (bExists)
    {
        bPersistenceHealthy = false;
        UE_LOG(LogTemp, Error, TEXT("Could not load native profile %s. Original slot preserved."), *PersistentPlayerId);
    }
    bDirty = false;
    SyncPublicProfile();
}

bool AAetheriaPlayerState::CommitIdentity(const FString& Name, FName Avatar)
{
    if (!HasAuthority() || PublicProfile.bIdentityCommitted || !bPersistenceHealthy) return false;
    FString Clean = Name.TrimStartAndEnd();
    if (Clean.IsEmpty() || Clean.Len() > 32 || (Avatar != TEXT("male") && Avatar != TEXT("female"))) return false;
    for (TCHAR C : Clean) if (C < 32 || C == 127) return false;
    const FTCHARToUTF8 EncodedName(*Clean);
    if (EncodedName.Length() > 32) return false;
    RulesState.name = std::string(EncodedName.Get(), EncodedName.Length());
    RulesState.appearance.sex = TCHAR_TO_UTF8(*Avatar.ToString());
    PublicProfile.bIdentityCommitted = true;
    bDirty = true;
    SyncPublicProfile();
    return SaveProfile();
}

void AAetheriaPlayerState::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !PublicProfile.bIdentityCommitted || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
    TickRemainderMs += static_cast<double>(DeltaSeconds) * 1000.0;
    const uint32 Elapsed = static_cast<uint32>(FMath::Min(TickRemainderMs, 10000.0));
    TickRemainderMs -= Elapsed;
    if (Elapsed) { R::Tick(RulesState, Elapsed); bDirty = true; }
    SinceProfileUpdate += DeltaSeconds;
    if (SinceProfileUpdate >= .2) { SinceProfileUpdate = 0; SyncPublicProfile(); }
    SinceLastSave += DeltaSeconds;
    if (SinceLastSave >= 10.0) { SinceLastSave = 0; if (bDirty) SaveProfile(); }
}

bool AAetheriaPlayerState::ApplyStoryEvent(R::StoryEvent Event)
{
    if (!HasAuthority() || !PublicProfile.bIdentityCommitted) return false;
    const R::Result Result = R::ApplyStoryEvent(RulesState, Event);
    if (Result.ok) { bDirty = true; SyncPublicProfile(); SaveProfile(); }
    return Result.ok;
}

bool AAetheriaPlayerState::RequestAwakening()
{
    if (!HasAuthority() || !PublicProfile.bIdentityCommitted) return false;
    // A full-width authority draw avoids bias toward Fire in multiply-high mapping.
    FRandomStream Random(static_cast<int32>(GetTypeHash(FGuid::NewGuid())));
    const R::Result Result = R::Awaken(RulesState, Random.GetUnsignedInt());
    if (Result.ok) { bDirty = true; SyncPublicProfile(); SaveProfile(); }
    return Result.ok;
}

bool AAetheriaPlayerState::TrySpendStamina(int32 Amount)
{
    if (!HasAuthority() || !PublicProfile.bIdentityCommitted) return false;
    const R::Result Result = R::SpendStamina(RulesState, Amount);
    if (Result.ok) { bDirty = true; SyncPublicProfile(); }
    return Result.ok;
}

bool AAetheriaPlayerState::IsAlive() const { return HasAuthority() ? RulesState.health > 0 : PublicProfile.Health > 0; }
uint64 AAetheriaPlayerState::GetServerTimeMs() const { return RulesState.serverTimeMs; }

R::Result AAetheriaPlayerState::ApplyServerDamage(int32 Amount)
{
    if (!HasAuthority()) { R::Result Result; Result.message = "Server authority required."; return Result; }
    R::Result Result = R::ApplyServerDamage(RulesState, Amount);
    if (Result.ok) { bDirty = true; SyncPublicProfile(); }
    return Result;
}

R::Result AAetheriaPlayerState::TryCast(const R::SpellSpec& Spec)
{
    if (!HasAuthority() || !PublicProfile.bIdentityCommitted) { R::Result Result; Result.message = "Server identity required."; return Result; }
    R::Result Result = R::Cast(RulesState, Spec, GetServerTimeMs());
    if (Result.ok) { bDirty = true; SyncPublicProfile(); }
    return Result;
}

bool AAetheriaPlayerState::ResolveSpell(FName Id, R::SpellSpec& Out) const
{
    if (RulesState.affinity == R::Element::None) return false;
    if (Id.IsNone() || Id == TEXT("Primary") || Id == TEXT("Default") || Id == TEXT("Basic"))
    {
        Out = R::DefaultSpell(RulesState.affinity);
        return true;
    }
    for (const R::NamedSpell& Spell : RulesState.spells)
    {
        if (Spell.phase == R::SpellPhase::Registered && Id == FName(*FString::Printf(TEXT("Spell_%u"), Spell.id))) { Out = Spell.spec; return true; }
    }
    return false;
}

void AAetheriaPlayerState::SyncPublicProfile()
{
    if (!HasAuthority()) return;
    PublicProfile.Name = UTF8_TO_TCHAR(RulesState.name.c_str());
    PublicProfile.Avatar = FName(UTF8_TO_TCHAR(RulesState.appearance.sex.c_str()));
    PublicProfile.Element = RulesState.affinity == R::Element::None ? -1 : static_cast<int32>(RulesState.affinity);
    PublicProfile.MasteryStage = PublicProfile.Element >= 0 ? static_cast<int32>(RulesState.mastery[PublicProfile.Element].stage) : 0;
    PublicProfile.Chapter = static_cast<int32>(RulesState.chapter);
    PublicProfile.Level = static_cast<int32>(RulesState.level);
    PublicProfile.Health = RulesState.health; PublicProfile.Mana = RulesState.mana; PublicProfile.Stamina = RulesState.stamina;
    PublicProfile.Fatigue = RulesState.fatigue;
    PublicProfile.Crowns = RulesState.wallet; PublicProfile.BankCrowns = RulesState.bank;
    PublicProfile.Objective = UTF8_TO_TCHAR(R::GetObjective(RulesState).c_str());
    if (RulesState.chapter == R::Chapter::FirstMagicTrial) PublicProfile.Objective += FString::Printf(TEXT("\nTargets %u/3 • Dodge %u/1 • Element focus %u/1"), RulesState.trialHits, RulesState.trialDodges, RulesState.trialInteractions);
    if (RulesState.chapter == R::Chapter::CityVisits)
    {
        const TCHAR* Names[] = {TEXT("Hospital"), TEXT("Academy"), TEXT("Guild"), TEXT("Houses"), TEXT("Market"), TEXT("Bank"), TEXT("Inn")};
        PublicProfile.Objective += TEXT("\nRemaining: ");
        for (int32 Index = 0; Index < 7; ++Index) if (!(RulesState.visitedFacilities & (1 << Index))) PublicProfile.Objective += FString(Names[Index]) + TEXT(" ");
    }
    PublicProfile.FinalChoice = static_cast<int32>(RulesState.finalChoice);
    ForceNetUpdate();
}

void AAetheriaPlayerState::OnRep_PublicProfile() {}
FString AAetheriaPlayerState::SaveSlot() const { return FString::Printf(TEXT("Aetheria_%s"), *PersistentPlayerId); }

bool AAetheriaPlayerState::SaveProfile()
{
    if (!HasAuthority() || !bPersistenceHealthy || PersistentPlayerId.IsEmpty()) return false;
    const std::vector<std::uint8_t> Bytes = R::EncodeSave(RulesState);
    if (Bytes.empty()) return false;
    UAetheriaSaveGame* Saved = Cast<UAetheriaSaveGame>(UGameplayStatics::CreateSaveGameObject(UAetheriaSaveGame::StaticClass()));
    Saved->PersistentPlayerId = PersistentPlayerId;
    Saved->bIdentityCommitted = PublicProfile.bIdentityCommitted;
    Saved->RulesBytes.Reserve(static_cast<int32>(Bytes.size()));
    for (std::uint8_t Byte : Bytes) Saved->RulesBytes.Add(Byte);
    Saved->Conversations = Conversations;
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Saved, SaveSlot(), 0);
    if (bSaved) bDirty = false;
    else UE_LOG(LogTemp, Warning, TEXT("Could not persist native profile %s."), *PersistentPlayerId);
    return bSaved;
}

FAetheriaUiNpcConversation& AAetheriaPlayerState::GetConversation(FName Id)
{
    check(HasAuthority());
    for (FAetheriaUiNpcConversation& Conversation : Conversations) if (Conversation.NpcId == Id) return Conversation;
    if (Conversations.Num() >= 32) Conversations.RemoveAt(0);
    FAetheriaUiNpcConversation& Conversation = Conversations.AddDefaulted_GetRef();
    Conversation.NpcId = Id;
    bDirty = true;
    return Conversation;
}

void AAetheriaPlayerState::RecordConversation(const FAetheriaUiNpcConversation& Input)
{
    if (!HasAuthority() || Input.NpcId.IsNone()) return;
    FAetheriaUiNpcConversation Copy = Input;
    if (Copy.Messages.Num() > 40) Copy.Messages.RemoveAt(0, Copy.Messages.Num() - 40);
    for (FAetheriaUiNpcMessage& Message : Copy.Messages) { Message.Text = Message.Text.Left(280); Message.Sender = Message.Sender.Left(64); }
    Copy.Familiarity = FMath::Clamp(Copy.Familiarity, 0, 100);
    Copy.bAwaitingReply = false;
    GetConversation(Input.NpcId) = MoveTemp(Copy);
    bDirty = true;
}
