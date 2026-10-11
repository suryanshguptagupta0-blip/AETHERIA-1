#include "UI/AetheriaRootWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SafeZone.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/Paths.h"
#include "Input/Reply.h"

void UAetheriaUiCommand::Execute() { if (Owner) Owner->Dispatch(Action); }
FString UAetheriaRootWidget::Tr(const TCHAR* English, const TCHAR* Hindi) const { return Locale == TEXT("hi") ? Hindi : English; }
UTextBlock* UAetheriaRootWidget::Text(const FString& Value, int32 Size)
{
    UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
    Result->SetText(FText::FromString(Value)); Result->SetAutoWrapText(true);
    Result->SetColorAndOpacity(FSlateColor(FLinearColor(.9f, .84f, .67f)));
    // The free source font is staged by config. Device shaping/cooking still needs UE QA.
    const FString Font = FPaths::ProjectContentDir() / TEXT("UI/Fonts/NotoSansDevanagari-Variable.ttf");
    if (FPaths::FileExists(Font)) Result->SetFont(FSlateFontInfo(Font, Size));
    return Result;
}
void UAetheriaRootWidget::Button(UVerticalBox* Box, const FString& Label, FName Action)
{
    UButton* Control = WidgetTree->ConstructWidget<UButton>();
    Control->SetContent(Text(Label, 16));
    Control->SetBackgroundColor(FLinearColor(.13f, .21f, .23f));
    UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Control); Slot->SetPadding(FMargin(0, 3));
    Slot->SetHorizontalAlignment(HAlign_Left);
    UAetheriaUiCommand* Command = NewObject<UAetheriaUiCommand>(this); Command->Owner = this; Command->Action = Action;
    Commands.Add(Command); Control->OnClicked.AddDynamic(Command, &UAetheriaUiCommand::Execute);
}
UBorder* UAetheriaRootWidget::Panel(UVerticalBox*& Body, bool bFull)
{
    UBorder* Result = WidgetTree->ConstructWidget<UBorder>();
    Result->SetBrushColor(FLinearColor(.018f, .038f, .048f, bFull ? .97f : .9f));
    Result->SetPadding(FMargin(20)); Body = WidgetTree->ConstructWidget<UVerticalBox>(); Result->SetContent(Body);
    UOverlaySlot* Slot = Layers->AddChildToOverlay(Result);
    Slot->SetHorizontalAlignment(bFull ? HAlign_Fill : HAlign_Center); Slot->SetVerticalAlignment(bFull ? VAlign_Fill : VAlign_Center);
    Result->SetVisibility(ESlateVisibility::Collapsed); return Result;
}
TSharedRef<SWidget> UAetheriaRootWidget::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
    USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>();
    Layers = WidgetTree->ConstructWidget<UOverlay>(); Safe->AddChild(Layers); WidgetTree->RootWidget = Safe;
    Commands.Reset();
    UVerticalBox* Body = nullptr;
    Menu = Panel(Body);
    Body->AddChildToVerticalBox(Text(TEXT("AETHERIA — FRACTURED REALITIES"), 26));
    Body->AddChildToVerticalBox(Text(Tr(TEXT("Create your traveler. Your journey begins at the hospital."), TEXT("अपना यात्री चुनें। यात्रा अस्पताल से शुरू होगी।"))));
    NameInput = WidgetTree->ConstructWidget<UEditableTextBox>(); NameInput->SetHintText(FText::FromString(Tr(TEXT("Your name"), TEXT("आपका नाम")))); Body->AddChildToVerticalBox(NameInput);
    NameInput->SetText(FText::FromString(TEXT("Aerin")));
    AvatarText = Text(Tr(TEXT("Choose a male or female avatar"), TEXT("पुरुष या महिला अवतार चुनें"))); Body->AddChildToVerticalBox(AvatarText);
    Button(Body, Tr(TEXT("Male"), TEXT("पुरुष")), TEXT("AvatarMale")); Button(Body, Tr(TEXT("Female"), TEXT("महिला")), TEXT("AvatarFemale"));
    Button(Body, Tr(TEXT("Begin / Continue"), TEXT("शुरू करें / जारी रखें")), TEXT("Play"));
    Button(Body, TEXT("English / हिन्दी"), TEXT("Language"));
    Hud = Panel(Body, true); Hud->SetBrushColor(FLinearColor(0, 0, 0, .001f));
    HudText = Text(TEXT(""), 17); Body->AddChildToVerticalBox(HudText);
    const TPair<const TCHAR*, const TCHAR*> Actions[] = {{TEXT("Interact / Talk [E]"), TEXT("Interact")}, {TEXT("Cast [F]"), TEXT("Cast")}, {TEXT("Dodge [Space]"), TEXT("Dodge")}, {TEXT("Jump"), TEXT("Jump")}, {TEXT("Sprint on"), TEXT("SprintDown")}, {TEXT("Sprint off"), TEXT("SprintUp")}, {TEXT("Settings"), TEXT("Settings")}};
    for (const auto& Action : Actions) Button(Body, Action.Key, FName(Action.Value));
    Button(Body, TEXT("City map [M]"), TEXT("Map"));
    Body->AddChildToVerticalBox(Text(TEXT("WASD / left touch: move • mouse/gamepad / right touch: look • Tab: target"), 13));
    Chat = Panel(Body, true);
    Body->AddChildToVerticalBox(Text(Tr(TEXT("City conversations"), TEXT("शहर में बातचीत")), 22));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Messages = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Messages);
    UVerticalBoxSlot* ChatSlot = Body->AddChildToVerticalBox(Scroll); ChatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(); ChatInput->SetHintText(FText::FromString(Tr(TEXT("Type a message (280 characters)"), TEXT("संदेश लिखें (280 अक्षर)"))));
    ChatInput->OnTextCommitted.AddDynamic(this, &UAetheriaRootWidget::CommitChat);
    ChatInput->OnTextChanged.AddDynamic(this, &UAetheriaRootWidget::ClampChat); Body->AddChildToVerticalBox(ChatInput);
    Button(Body, Tr(TEXT("Send"), TEXT("भेजें")), TEXT("Send"));
    Services = WidgetTree->ConstructWidget<UVerticalBox>(); Body->AddChildToVerticalBox(Services);
    Button(Body, Tr(TEXT("Return to city"), TEXT("शहर में लौटें")), TEXT("CloseChat"));
    Settings = Panel(Body);
    Body->AddChildToVerticalBox(Text(Tr(TEXT("Game settings"), TEXT("खेल की सेटिंग")), 24));
    Button(Body, TEXT("Performance"), TEXT("QualityLow")); Button(Body, TEXT("High detail"), TEXT("QualityHigh")); Button(Body, TEXT("Device default"), TEXT("QualityAuto"));
    Button(Body, TEXT("English / हिन्दी"), TEXT("Language")); Button(Body, Tr(TEXT("Back"), TEXT("वापस")), TEXT("CloseSettings"));
    Map = Panel(Body, true);
    Body->AddChildToVerticalBox(Text(TEXT("AETHERIA • City map"), 22));
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    UVerticalBoxSlot* MapSlot = Body->AddChildToVerticalBox(Canvas); MapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const TCHAR* PlaceNames[] = {TEXT("Hospital"), TEXT("Awakening Hall"), TEXT("Academy"), TEXT("Guild"), TEXT("Houses"), TEXT("Market"), TEXT("Bank"), TEXT("Inn"), TEXT("Library"), TEXT("Magic Trial"), TEXT("Guardian")};
    const FVector2D Points[] = {{-5000,-3200},{-1600,-3500},{2000,-3200},{-4500,3200},{-1000,3100},{2500,3100},{6200,-3500},{6200,3300},{-5100,-6600},{5200,0},{11200,0}};
    for (int32 Index = 0; Index < 11; ++Index)
    {
        UBorder* Marker = WidgetTree->ConstructWidget<UBorder>(); Marker->SetBrushColor(FLinearColor(.14, .23, .25)); Marker->SetContent(Text(PlaceNames[Index], 12));
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Marker);
        Slot->SetAnchors(FAnchors((Points[Index].X + 8000) / 22000, (Points[Index].Y + 8500) / 15000));
        Slot->SetAlignment(FVector2D(.5, .5)); Slot->SetSize(FVector2D(110, 40));
    }
    UBorder* PlayerMarker = WidgetTree->ConstructWidget<UBorder>(); PlayerMarker->SetBrushColor(FLinearColor(.2, .9, .9));
    PlayerMapSlot = Canvas->AddChildToCanvas(PlayerMarker); PlayerMapSlot->SetSize(FVector2D(10, 10)); PlayerMapSlot->SetAlignment(FVector2D(.5, .5));
    Button(Body, TEXT("Return to city"), TEXT("CloseMap"));
    StatusText = Text(TEXT(""), 14); UOverlaySlot* StatusSlot = Layers->AddChildToOverlay(StatusText);
    StatusSlot->SetHorizontalAlignment(HAlign_Center); StatusSlot->SetVerticalAlignment(VAlign_Bottom); StatusSlot->SetPadding(FMargin(16));
    StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
    ShowMenu(); return Super::RebuildWidget();
}
void UAetheriaRootWidget::SetCharacterIdentity(const FString& Name, FName Avatar)
{
    bIdentityCommitted = true; SelectedAvatar = Avatar;
    if (NameInput) { NameInput->SetText(FText::FromString(Name)); NameInput->SetIsReadOnly(true); }
    if (AvatarText) AvatarText->SetText(FText::FromString(Avatar.ToString()));
}
void UAetheriaRootWidget::ShowFeedback(const FString& Value) { if (StatusText) StatusText->SetText(FText::FromString(Value.Left(280))); }
void UAetheriaRootWidget::SetHud(const FAetheriaUiHudSnapshot& Value)
{
    if (PlayerMapSlot) PlayerMapSlot->SetAnchors(FAnchors(FMath::Clamp((Value.PlayerMapPosition.X + 8000) / 22000, 0.0, 1.0), FMath::Clamp((Value.PlayerMapPosition.Y + 8500) / 15000, 0.0, 1.0)));
    if (!HudText) return;
    HudText->SetText(FText::FromString(FString::Printf(TEXT("%s • Level %d • %lld crowns\nHP %.0f    Mana %.0f    Stamina %.0f\n%s • Stage %d\n%s\n%s"), *Value.PlayerName, Value.Level, static_cast<long long>(Value.Gold), Value.Health, Value.Mana, Value.Stamina, *Value.ElementId.ToString(), Value.MasteryStage + 1, *Value.Location, *Value.Objective)));
}
void UAetheriaRootWidget::SetNpcConversation(const FAetheriaUiNpcConversation& Value)
{
    Conversation = Value;
    if (!Messages || !Services) return;
    Messages->ClearChildren(); Services->ClearChildren();
    Commands.RemoveAll([](const TObjectPtr<UAetheriaUiCommand>& Command) { return Command && (Command->Action.ToString().StartsWith(TEXT("Service_")) || Command->Action == TEXT("Seal") || Command->Action == TEXT("Study")); });
    Messages->AddChildToVerticalBox(Text(Value.Name + TEXT(" • ") + Value.Role, 20));
    Messages->AddChildToVerticalBox(Text(Value.Family, 13));
    for (const auto& Message : Value.Messages) Messages->AddChildToVerticalBox(Text(Message.Sender + TEXT(": ") + Message.Text));
    for (const auto& Service : Value.Services)
    {
        const FName Action = Service.Action == TEXT("Seal") || Service.Action == TEXT("Study") ? Service.Action : FName(*(TEXT("Service_") + Service.Action.ToString()));
        Button(Services, Service.Label, Action);
    }
}
void UAetheriaRootWidget::ReleaseInputs() { MovePointer = LookPointer = INDEX_NONE; OnMove.Broadcast(FVector2D::ZeroVector); OnLook.Broadcast(FVector2D::ZeroVector); OnAction.Broadcast(TEXT("SprintUp")); }
void UAetheriaRootWidget::ShowMap() { ShowHud(); ReleaseInputs(); bHudVisible = false; if (Hud) Hud->SetVisibility(ESlateVisibility::Collapsed); if (Map) Map->SetVisibility(ESlateVisibility::Visible); }
void UAetheriaRootWidget::ShowMenu() { if (Map) Map->SetVisibility(ESlateVisibility::Collapsed); ReleaseInputs(); bHudVisible = false; if (Menu) Menu->SetVisibility(ESlateVisibility::Visible); if (Hud) Hud->SetVisibility(ESlateVisibility::Collapsed); if (Chat) Chat->SetVisibility(ESlateVisibility::Collapsed); if (Settings) Settings->SetVisibility(ESlateVisibility::Collapsed); }
void UAetheriaRootWidget::ShowHud() { if (Map) Map->SetVisibility(ESlateVisibility::Collapsed); ReleaseInputs(); bHudVisible = true; if (Menu) Menu->SetVisibility(ESlateVisibility::Collapsed); if (Chat) Chat->SetVisibility(ESlateVisibility::Collapsed); if (Settings) Settings->SetVisibility(ESlateVisibility::Collapsed); if (Hud) Hud->SetVisibility(ESlateVisibility::Visible); }
void UAetheriaRootWidget::ShowConversation() { if (Map) Map->SetVisibility(ESlateVisibility::Collapsed); ReleaseInputs(); bHudVisible = false; if (Hud) Hud->SetVisibility(ESlateVisibility::Collapsed); if (Menu) Menu->SetVisibility(ESlateVisibility::Collapsed); if (Settings) Settings->SetVisibility(ESlateVisibility::Collapsed); if (Chat) Chat->SetVisibility(ESlateVisibility::Visible); if (ChatInput) ChatInput->SetKeyboardFocus(); }
void UAetheriaRootWidget::ShowSettings(bool bShow) { if (Map) Map->SetVisibility(ESlateVisibility::Collapsed); if (!bShow) { ShowHud(); return; } ReleaseInputs(); bHudVisible = false; if (Hud) Hud->SetVisibility(ESlateVisibility::Collapsed); if (Settings) Settings->SetVisibility(ESlateVisibility::Visible); }
void UAetheriaRootWidget::SetInputMode(bool bTouch) { bTouchInput = bTouch; }
void UAetheriaRootWidget::SetLocale(FName Value)
{
    Locale = Value == TEXT("hi") ? FName(TEXT("hi")) : FName(TEXT("en"));
    const TPair<const TCHAR*, const TCHAR*> Labels[] = {
        {TEXT("Create your traveler. Your journey begins at the hospital."), TEXT("अपना यात्री चुनें। यात्रा अस्पताल से शुरू होगी।")},
        {TEXT("Male"), TEXT("पुरुष")}, {TEXT("Female"), TEXT("महिला")}, {TEXT("Begin / Continue"), TEXT("शुरू करें / जारी रखें")},
        {TEXT("City conversations"), TEXT("शहर में बातचीत")}, {TEXT("Send"), TEXT("भेजें")}, {TEXT("Return to city"), TEXT("शहर में लौटें")},
        {TEXT("Game settings"), TEXT("खेल की सेटिंग")}, {TEXT("Back"), TEXT("वापस")}
    };
    if (WidgetTree) WidgetTree->ForEachWidget([&](UWidget* Widget) { if (UTextBlock* Label = Cast<UTextBlock>(Widget)) for (const auto& Pair : Labels) if (Label->GetText().ToString() == Pair.Key || Label->GetText().ToString() == Pair.Value) { Label->SetText(FText::FromString(Locale == TEXT("hi") ? Pair.Value : Pair.Key)); break; } });
}
void UAetheriaRootWidget::Dispatch(FName Action)
{
    if (Action == TEXT("AvatarMale") || Action == TEXT("AvatarFemale")) { if (bIdentityCommitted) return; SelectedAvatar = Action == TEXT("AvatarMale") ? FName(TEXT("male")) : FName(TEXT("female")); if (AvatarText) AvatarText->SetText(FText::FromString(SelectedAvatar.ToString())); return; }
    if (Action == TEXT("Play")) { if (bIdentityCommitted) OnAction.Broadcast(Action); else if (SelectedAvatar.IsNone()) ShowFeedback(Tr(TEXT("Choose a male or female avatar first."), TEXT("पहले पुरुष या महिला अवतार चुनें।"))); else OnCreateCharacter.Broadcast(NameInput ? NameInput->GetText().ToString() : TEXT("Traveler"), SelectedAvatar); return; }
    if (Action == TEXT("Send")) { const FString Message = ChatInput ? ChatInput->GetText().ToString().TrimStartAndEnd().Left(280) : FString(); if (!Message.IsEmpty() && !Conversation.NpcId.IsNone()) { OnSendNpcMessage.Broadcast(Conversation.NpcId, Message); ChatInput->SetText(FText::GetEmpty()); } return; }
    if (Action == TEXT("Language")) { SetLocale(Locale == TEXT("en") ? FName(TEXT("hi")) : FName(TEXT("en"))); if (AvatarText) AvatarText->SetText(FText::FromString(Tr(TEXT("Language: English"), TEXT("भाषा: हिन्दी")))); return; }
    OnAction.Broadcast(Action);
}
void UAetheriaRootWidget::CommitChat(const FText& Value, ETextCommit::Type Method) { if (Method == ETextCommit::OnEnter) Dispatch(TEXT("Send")); }
void UAetheriaRootWidget::ClampChat(const FText& Value) { if (ChatInput && Value.ToString().Len() > 280) ChatInput->SetText(FText::FromString(Value.ToString().Left(280))); }
FReply UAetheriaRootWidget::NativeOnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bHudVisible || !bTouchInput) return Super::NativeOnTouchStarted(Geometry, Event);
    const FVector2D Point = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if (Point.X < Geometry.GetLocalSize().X * .35f && MovePointer == INDEX_NONE) { MovePointer = Event.GetPointerIndex(); MoveOrigin = Point; return FReply::Handled(); }
    if (Point.X > Geometry.GetLocalSize().X * .5f && LookPointer == INDEX_NONE) { LookPointer = Event.GetPointerIndex(); LastLook = Point; return FReply::Handled(); }
    return Super::NativeOnTouchStarted(Geometry, Event);
}
FReply UAetheriaRootWidget::NativeOnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event)
{
    const FVector2D Point = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if (bHudVisible && Event.GetPointerIndex() == MovePointer) { FVector2D Value = (Point - MoveOrigin) / 85.f; Value.Y *= -1; if (Value.SizeSquared() > 1) Value.Normalize(); OnMove.Broadcast(Value); return FReply::Handled(); }
    if (bHudVisible && Event.GetPointerIndex() == LookPointer) { OnLook.Broadcast((Point - LastLook) * FVector2D(.12f, -.12f)); LastLook = Point; return FReply::Handled(); }
    return Super::NativeOnTouchMoved(Geometry, Event);
}
FReply UAetheriaRootWidget::NativeOnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetPointerIndex() == MovePointer) { MovePointer = INDEX_NONE; OnMove.Broadcast(FVector2D::ZeroVector); return FReply::Handled(); }
    if (Event.GetPointerIndex() == LookPointer) { LookPointer = INDEX_NONE; OnLook.Broadcast(FVector2D::ZeroVector); return FReply::Handled(); }
    return Super::NativeOnTouchEnded(Geometry, Event);
}
void UAetheriaRootWidget::NativeDestruct() { ReleaseInputs(); Super::NativeDestruct(); }
void UAetheriaRootWidget::NativeOnFocusLost(const FFocusEvent& Event) { ReleaseInputs(); Super::NativeOnFocusLost(Event); }
