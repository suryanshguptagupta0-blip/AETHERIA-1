#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AetheriaGameMode.generated.h"

class AAetheriaWorldRuntime;

UCLASS(Config=Game)
class AETHERIA_API AAetheriaGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAetheriaGameMode();
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    AAetheriaWorldRuntime* GetRuntimeDistrict() const { return RuntimeDistrict; }
    UPROPERTY(Config) bool bAllowDevelopmentProfiles = false;
    UPROPERTY(Config) int32 MaximumDistrictPlayers = 8;
private:
    UPROPERTY() TObjectPtr<AAetheriaWorldRuntime> RuntimeDistrict;
    UPROPERTY() TObjectPtr<AActor> HospitalPlayerStart;
    TSet<FString> ConnectedProfiles;
    void EnsureDistrict();
};
