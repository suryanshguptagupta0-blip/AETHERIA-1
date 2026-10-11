#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/AetheriaUiTypes.h"
#include "Types/SlateEnums.h"
#include "AetheriaRootWidget.generated.h"

class UTextBlock; class UEditableTextBox; class UVerticalBox; class UOverlay; class UBorder;
class UCanvasPanelSlot;
class UAetheriaRootWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAetheriaCreateCharacter, const FString&, Name, FName, Avatar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAetheriaSendNpcMessage, FName, NpcId, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAetheriaUiAction, FName, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAetheriaUiVector, FVector2D, Value);

UCLASS()
class AETHERIA_API UAetheriaUiCommand : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UAetheriaRootWidget> Owner;
    FName Action;
    UFUNCTION() void Execute();
};

UCLASS()
class AETHERIA_API UAetheriaRootWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FAetheriaCreateCharacter OnCreateCharacter;
    UPROPERTY(BlueprintAssignable) FAetheriaSendNpcMessage OnSendNpcMessage;
    UPROPERTY(BlueprintAssignable) FAetheriaUiAction OnAction;
    UPROPERTY(BlueprintAssignable) FAetheriaUiVector OnMove;
    UPROPERTY(BlueprintAssignable) FAetheriaUiVector OnLook;
    void SetHud(const FAetheriaUiHudSnapshot& Value);
    void SetNpcConversation(const FAetheriaUiNpcConversation& Value);
    void SetCharacterIdentity(const FString& Name, FName Avatar);
    void ShowMenu(); void ShowHud(); void ShowConversation(); void ShowSettings(bool bShow);
    void ShowMap();
    void ShowFeedback(const FString& Value);
    void SetInputMode(bool bTouch); void SetLocale(FName Value); FName GetLocale() const { return Locale; }
    void Dispatch(FName Action);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual void NativeOnFocusLost(const FFocusEvent& Event) override;
    UFUNCTION() void CommitChat(const FText& Value, ETextCommit::Type Method);
    UFUNCTION() void ClampChat(const FText& Value);
private:
    UPROPERTY() TObjectPtr<UOverlay> Layers;
    UPROPERTY() TObjectPtr<UBorder> Menu;
    UPROPERTY() TObjectPtr<UBorder> Hud;
    UPROPERTY() TObjectPtr<UBorder> Chat;
    UPROPERTY() TObjectPtr<UBorder> Settings;
    UPROPERTY() TObjectPtr<UBorder> Map;
    UPROPERTY() TObjectPtr<UCanvasPanelSlot> PlayerMapSlot;
    UPROPERTY() TObjectPtr<UTextBlock> HudText;
    UPROPERTY() TObjectPtr<UTextBlock> AvatarText;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UEditableTextBox> NameInput;
    UPROPERTY() TObjectPtr<UEditableTextBox> ChatInput;
    UPROPERTY() TObjectPtr<UVerticalBox> Messages;
    UPROPERTY() TObjectPtr<UVerticalBox> Services;
    UPROPERTY() TArray<TObjectPtr<UAetheriaUiCommand>> Commands;
    FAetheriaUiNpcConversation Conversation;
    FName SelectedAvatar = NAME_None, Locale = TEXT("en");
    bool bIdentityCommitted = false, bTouchInput = false, bHudVisible = false;
    int32 MovePointer = INDEX_NONE, LookPointer = INDEX_NONE;
    FVector2D MoveOrigin = FVector2D::ZeroVector, LastLook = FVector2D::ZeroVector;
    UTextBlock* Text(const FString& Value, int32 Size = 16);
    void Button(UVerticalBox* Box, const FString& Label, FName Action);
    UBorder* Panel(UVerticalBox*& Body, bool bFull = false);
    FString Tr(const TCHAR* English, const TCHAR* Hindi) const;
    void ReleaseInputs();
};
