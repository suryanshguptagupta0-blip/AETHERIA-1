#include "Core/AetheriaGameMode.h"
#include "Core/AetheriaPlayerController.h"
#include "Core/AetheriaPlayerState.h"
#include "Characters/AetheriaCharacter.h"
#include "World/AetheriaWorldRuntime.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Persistence/AetheriaSaveGame.h"

AAetheriaGameMode::AAetheriaGameMode()
{
    DefaultPawnClass = AAetheriaCharacter::StaticClass();
    PlayerControllerClass = AAetheriaPlayerController::StaticClass();
    PlayerStateClass = AAetheriaPlayerState::StaticClass();
}

void AAetheriaGameMode::EnsureDistrict()
{
    if (!RuntimeDistrict)
    {
        RuntimeDistrict = Cast<AAetheriaWorldRuntime>(UGameplayStatics::GetActorOfClass(this, AAetheriaWorldRuntime::StaticClass()));
        if (!RuntimeDistrict) RuntimeDistrict = GetWorld()->SpawnActor<AAetheriaWorldRuntime>();
        if (RuntimeDistrict) RuntimeDistrict->InitializeRuntime();
    }
    if (!HospitalPlayerStart && RuntimeDistrict)
    {
        HospitalPlayerStart = GetWorld()->SpawnActor<APlayerStart>(RuntimeDistrict->GetHospitalSpawn(), RuntimeDistrict->GetHospitalSpawnRotation());
    }
}

void AAetheriaGameMode::StartPlay()
{
    EnsureDistrict();
    Super::StartPlay();
}

AActor* AAetheriaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    EnsureDistrict();
    return HospitalPlayerStart ? HospitalPlayerStart.Get() : Super::ChoosePlayerStart_Implementation(Player);
}

void AAetheriaGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (GetNumPlayers() >= FMath::Clamp(MaximumDistrictPlayers, 1, 8)) ErrorMessage = TEXT("The starting district is full.");
    // Development identities are convenient for LAN reconnect tests, never authentication.
    const FString Requested = UGameplayStatics::ParseOption(Options, TEXT("DevProfile"));
    FGuid Parsed;
    if (!Requested.IsEmpty() && (!bAllowDevelopmentProfiles || !FGuid::Parse(Requested, Parsed))) ErrorMessage = TEXT("Development profiles are unavailable or invalid.");
    if (Parsed.IsValid() && ConnectedProfiles.Contains(Parsed.ToString(EGuidFormats::DigitsWithHyphens))) ErrorMessage = TEXT("That development profile is already connected.");
}

APlayerController* AAetheriaGameMode::Login(UPlayer* NewPlayer, ENetRole RemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    if (ConnectedProfiles.Num() >= FMath::Clamp(MaximumDistrictPlayers, 1, 8)) { ErrorMessage = TEXT("The starting district is full."); return nullptr; }
    FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    if (GetNetMode() == NM_Standalone)
    {
        if (const UAetheriaSaveGame* LocalIdentity = Cast<UAetheriaSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("AetheriaLocalIdentity"), 0)))
        {
            FGuid Existing;
            if (FGuid::Parse(LocalIdentity->PersistentPlayerId, Existing) && Existing.IsValid()) Id = Existing.ToString(EGuidFormats::DigitsWithHyphens);
        }
    }
    const FString Requested = UGameplayStatics::ParseOption(Options, TEXT("DevProfile"));
    FGuid Parsed;
    if (!Requested.IsEmpty())
    {
        if (!bAllowDevelopmentProfiles || !FGuid::Parse(Requested, Parsed) || !Parsed.IsValid()) { ErrorMessage = TEXT("Invalid development profile."); return nullptr; }
        Id = Parsed.ToString(EGuidFormats::DigitsWithHyphens);
    }
    if (ConnectedProfiles.Contains(Id)) { ErrorMessage = TEXT("That profile is already connected."); return nullptr; }
    APlayerController* Controller = Super::Login(NewPlayer, RemoteRole, Portal, Options, UniqueId, ErrorMessage);
    if (AAetheriaPlayerController* NativeController = Cast<AAetheriaPlayerController>(Controller))
    {
        NativeController->SetPendingProfileId(Id);
        ConnectedProfiles.Add(Id);
        if (GetNetMode() == NM_Standalone)
        {
            UAetheriaSaveGame* Identity = Cast<UAetheriaSaveGame>(UGameplayStatics::CreateSaveGameObject(UAetheriaSaveGame::StaticClass()));
            Identity->PersistentPlayerId = Id;
            UGameplayStatics::SaveGameToSlot(Identity, TEXT("AetheriaLocalIdentity"), 0);
        }
    }
    return Controller;
}

void AAetheriaGameMode::PostLogin(APlayerController* NewPlayer)
{
    EnsureDistrict();
    if (AAetheriaPlayerController* Controller = Cast<AAetheriaPlayerController>(NewPlayer))
        if (AAetheriaPlayerState* State = Controller->GetPlayerState<AAetheriaPlayerState>()) State->InitializeProfile(Controller->GetPendingProfileId());
    Super::PostLogin(NewPlayer);
}

void AAetheriaGameMode::Logout(AController* Exiting)
{
    if (AAetheriaPlayerState* State = Exiting ? Exiting->GetPlayerState<AAetheriaPlayerState>() : nullptr)
    {
        State->SaveProfile();
        ConnectedProfiles.Remove(State->GetPersistentPlayerId());
    }
    Super::Logout(Exiting);
}

void AAetheriaGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* Controller = It->Get())
            if (AAetheriaPlayerState* State = Controller->GetPlayerState<AAetheriaPlayerState>()) State->SaveProfile();
    Super::EndPlay(Reason);
}
