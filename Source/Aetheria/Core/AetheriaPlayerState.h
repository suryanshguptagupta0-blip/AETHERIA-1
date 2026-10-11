#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/AetheriaPublicProfile.h"
#include "Rules/AetheriaRules.h"
#include "UI/AetheriaUiTypes.h"
#include "AetheriaPlayerState.generated.h"

UCLASS()
class AETHERIA_API AAetheriaPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    AAetheriaPlayerState();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    const Aetheria::Rules::State& GetRules() const { return RulesState; }
    Aetheria::Rules::State& MutableRules();
    const FAetheriaPublicProfile& GetPublicProfile() const { return PublicProfile; }
    FString GetPersistentPlayerId() const { return PersistentPlayerId; }
    void InitializeProfile(const FString& Id);
    bool CommitIdentity(const FString& Name, FName Avatar);
    bool ApplyStoryEvent(Aetheria::Rules::StoryEvent Event);
    bool RequestAwakening();
    bool TrySpendStamina(int32 Amount);
    bool IsAlive() const;
    Aetheria::Rules::Result ApplyServerDamage(int32 Amount);
    Aetheria::Rules::Result TryCast(const Aetheria::Rules::SpellSpec& Spec);
    bool ResolveSpell(FName Id, Aetheria::Rules::SpellSpec& Out) const;
    uint64 GetServerTimeMs() const;
    void SyncPublicProfile();
    bool SaveProfile();
    FAetheriaUiNpcConversation& GetConversation(FName NpcId);
    void RecordConversation(const FAetheriaUiNpcConversation& Conversation);
    void MarkDirty() { bDirty = true; }
protected:
    UPROPERTY(ReplicatedUsing=OnRep_PublicProfile) FAetheriaPublicProfile PublicProfile;
    UPROPERTY(Replicated) FString PersistentPlayerId;
    UFUNCTION() void OnRep_PublicProfile();
private:
    Aetheria::Rules::State RulesState;
    UPROPERTY() TArray<FAetheriaUiNpcConversation> Conversations;
    bool bDirty = false;
    bool bPersistenceHealthy = true;
    double TickRemainderMs = 0;
    double SinceLastSave = 0;
    double SinceProfileUpdate = 0;
    FString SaveSlot() const;
};
