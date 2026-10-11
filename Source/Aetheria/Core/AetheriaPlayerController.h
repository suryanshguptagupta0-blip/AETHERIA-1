#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/AetheriaUiTypes.h"
#include "AetheriaPlayerController.generated.h"

class UAetheriaRootWidget;
class AAetheriaNpcActor;
class AAetheriaPlayerState;

UCLASS()
class AETHERIA_API AAetheriaPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AAetheriaPlayerController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(Server, Reliable) void ServerCommitIdentity(const FString& Name, FName Avatar);
    UFUNCTION(Server, Reliable) void ServerBeginJourney();
    UFUNCTION(Server, Reliable) void ServerSetUiEditing(bool bEditing);
    UFUNCTION(Server, Reliable) void ServerInteract(AActor* Target);
    UFUNCTION(Server, Reliable) void ServerNpcMessage(FName NpcId, const FString& Message);
    UFUNCTION(Server, Reliable) void ServerRequestService(FName NpcId, FName Action);
    UFUNCTION(Server, Reliable) void ServerChooseFinal(bool bSeal);
    UFUNCTION(Client, Reliable) void ClientConversation(const FAetheriaUiNpcConversation& Conversation);
    UFUNCTION(Client, Reliable) void ClientBeginJourney();
    UFUNCTION(Client, Reliable) void ClientFeedback(const FString& Message);
    bool IsGameplayInputAllowed() const { return bJourneyEntered && !bEditingUi; }
    void HandleKnockout();
    void OpenCityMap() { HandleUiAction(TEXT("Map")); }
    void ReceiveNpcReply(AAetheriaNpcActor* Npc, const FString& PlayerText, const FText& Reply);
    FString GetPendingProfileId() const { return PendingProfileId; }
    void SetPendingProfileId(const FString& Id) { PendingProfileId = Id; }
protected:
    UPROPERTY() TObjectPtr<UAetheriaRootWidget> RootWidget;
    UPROPERTY() FName ActiveNpc = NAME_None;
    UFUNCTION() void HandleCreateCharacter(const FString& Name, FName Avatar);
    UFUNCTION() void HandleNpcMessage(FName NpcId, const FString& Message);
    UFUNCTION() void HandleUiAction(FName Action);
    UFUNCTION() void HandleMove(FVector2D Value);
    UFUNCTION() void HandleLook(FVector2D Value);
private:
    FString PendingProfileId;
    bool bJourneyEntered = false;
    bool bEditingUi = true;
    bool bHadReplicatedIdentity = false;
    double LastInteraction = -10;
    double LastNpcMessage = -10;
    double LastService = -10;
    double HudAccumulator = 0;
    double FeedbackUntil = 0;
    FString PendingFeedback;
    void SetUiEditing(bool bEditing);
    bool IsNear(AActor* Target, float Range) const;
    AAetheriaNpcActor* FindNpc(FName Id) const;
    AAetheriaPlayerState* Profile() const;
    void UpdateHud();
};
