#include "Characters/AetheriaCharacter.h"
#include "Engine/StaticMesh.h"
#include "Characters/AetheriaCharacterMovement.h"
#include "Core/AetheriaPlayerController.h"
#include "Core/AetheriaPlayerState.h"
#include "World/AetheriaInteractable.h"
#include "World/AetheriaWorldRuntime.h"
#include "Combat/AetheriaProjectile.h"
#include "AI/AetheriaGuardian.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AAetheriaCharacter::AAetheriaCharacter(const FObjectInitializer& Initializer)
    : Super(Initializer.SetDefaultSubobjectClass<UAetheriaCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true; bReplicates = true; bUseControllerRotationYaw = false;
    GetCharacterMovement()->MaxWalkSpeed = 420; GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 540, 0); GetCharacterMovement()->JumpZVelocity = 480;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom")); CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 430; CameraBoom->SocketOffset = FVector(0, 60, 65); CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true; CameraBoom->CameraLagSpeed = 12; CameraBoom->bEnableCameraRotationLag = true; CameraBoom->CameraRotationLagSpeed = 14;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera")); Camera->SetupAttachment(CameraBoom); Camera->FieldOfView = 72;
    EngineeringBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EngineeringBody")); EngineeringBody->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) EngineeringBody->SetStaticMesh(Sphere.Object);
    EngineeringBody->SetRelativeScale3D(FVector(.55, .55, 1.5)); EngineeringBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
bool AAetheriaCharacter::CanAct() const
{
    const AAetheriaPlayerController* Controller = Cast<AAetheriaPlayerController>(GetController());
    const AAetheriaPlayerState* State = GetPlayerState<AAetheriaPlayerState>();
    return Controller && Controller->IsGameplayInputAllowed() && State && State->IsAlive();
}
bool AAetheriaCharacter::CanJumpInternal_Implementation() const { return CanAct() && Super::CanJumpInternal_Implementation(); }
void AAetheriaCharacter::CreateMapping()
{
    if (Mapping) return;
    Mapping = NewObject<UInputMappingContext>(this);
    auto Action = [this](EInputActionValueType Type) { UInputAction* Result = NewObject<UInputAction>(this); Result->ValueType = Type; return Result; };
    MoveAction = Action(EInputActionValueType::Axis2D); LookAction = Action(EInputActionValueType::Axis2D);
    InteractAction = Action(EInputActionValueType::Boolean); CastAction = Action(EInputActionValueType::Boolean);
    DodgeAction = Action(EInputActionValueType::Boolean); SprintAction = Action(EInputActionValueType::Boolean); TargetAction = Action(EInputActionValueType::Boolean);
    MapAction = Action(EInputActionValueType::Boolean);
    auto Key = [this](UInputAction* Input, FKey Value, bool bY = false, bool bNegative = false)
    {
        FEnhancedActionKeyMapping& Entry = Mapping->MapKey(Input, Value);
        if (bY) { UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping); Swizzle->Order = EInputAxisSwizzle::YXZ; Entry.Modifiers.Add(Swizzle); }
        if (bNegative) Entry.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
    };
    Key(MoveAction, EKeys::W, true); Key(MoveAction, EKeys::S, true, true); Key(MoveAction, EKeys::D); Key(MoveAction, EKeys::A, false, true);
    Key(MoveAction, EKeys::Gamepad_Left2D); Key(LookAction, EKeys::Mouse2D); Key(LookAction, EKeys::Gamepad_Right2D);
    Key(InteractAction, EKeys::E); Key(InteractAction, EKeys::Gamepad_FaceButton_Left);
    Key(CastAction, EKeys::F); Key(CastAction, EKeys::Gamepad_RightTrigger);
    Key(DodgeAction, EKeys::SpaceBar); Key(DodgeAction, EKeys::Gamepad_FaceButton_Bottom);
    Key(SprintAction, EKeys::LeftShift); Key(SprintAction, EKeys::Gamepad_LeftThumbstick); Key(TargetAction, EKeys::Tab);
    Key(MapAction, EKeys::M);
}
void AAetheriaCharacter::BeginPlay()
{
    Super::BeginPlay(); CreateMapping();
    if (APlayerController* Controller = Cast<APlayerController>(GetController()))
        if (ULocalPlayer* Local = Controller->GetLocalPlayer())
            if (UEnhancedInputLocalPlayerSubsystem* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) Input->AddMappingContext(Mapping, 0);
}
void AAetheriaCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input); if (!Mapping) CreateMapping();
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
        if (ULocalPlayer* Local = Player->GetLocalPlayer())
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) Subsystem->AddMappingContext(Mapping, 0);
    if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(Input))
    {
        Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAetheriaCharacter::Move);
        Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAetheriaCharacter::Look);
        Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &AAetheriaCharacter::PerformInteract);
        Enhanced->BindAction(CastAction, ETriggerEvent::Started, this, &AAetheriaCharacter::CastRequested);
        Enhanced->BindAction(DodgeAction, ETriggerEvent::Started, this, &AAetheriaCharacter::DodgePressed);
        Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &AAetheriaCharacter::SprintPressed);
        Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &AAetheriaCharacter::SprintReleased);
        Enhanced->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AAetheriaCharacter::SprintReleased);
        Enhanced->BindAction(TargetAction, ETriggerEvent::Started, this, &AAetheriaCharacter::TargetPressed);
        Enhanced->BindAction(MapAction, ETriggerEvent::Started, this, &AAetheriaCharacter::MapPressed);
    }
}
void AAetheriaCharacter::ApplyMove(FVector2D Value)
{
    if (!CanAct() || !Controller) return;
    const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), FMath::Clamp(Value.Y, -1.f, 1.f));
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), FMath::Clamp(Value.X, -1.f, 1.f));
}
void AAetheriaCharacter::Move(const FInputActionValue& Value) { ApplyMove(Value.Get<FVector2D>()); }
void AAetheriaCharacter::Look(const FInputActionValue& Value) { SetTouchLook(Value.Get<FVector2D>()); }
void AAetheriaCharacter::SetTouchMove(FVector2D Value) { TouchMove = CanAct() && !Value.ContainsNaN() ? Value : FVector2D::ZeroVector; if (TouchMove.SizeSquared() > 1) TouchMove.Normalize(); }
void AAetheriaCharacter::SetTouchLook(FVector2D Value) { if (CanAct() && !Value.ContainsNaN()) { AddControllerYawInput(FMath::Clamp(Value.X, -12.f, 12.f)); AddControllerPitchInput(FMath::Clamp(Value.Y, -12.f, 12.f)); } }
void AAetheriaCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); UpdateAvatar(); if (IsLocallyControlled()) ApplyMove(TouchMove);
    if (!HasAuthority()) return;
    const FVector Position = GetActorLocation();
    if (Position.Z < -500 || FMath::Abs(Position.X - 2500) > 17500 || FMath::Abs(Position.Y) > 11500)
    {
        if (AAetheriaWorldRuntime* District = Cast<AAetheriaWorldRuntime>(UGameplayStatics::GetActorOfClass(this, AAetheriaWorldRuntime::StaticClass()))) TeleportTo(District->GetHospitalSpawn(), District->GetHospitalSpawnRotation());
        GetCharacterMovement()->StopMovementImmediately();
    }
    UAetheriaCharacterMovement* Movement = CastChecked<UAetheriaCharacterMovement>(GetCharacterMovement());
    if (Movement->bWantsToSprint && GetVelocity().SizeSquared2D() > 100 && CanAct())
    {
        SprintDrain += DeltaSeconds * 12;
        const int32 Cost = FMath::FloorToInt(SprintDrain);
        if (Cost > 0) { SprintDrain -= Cost; if (AAetheriaPlayerState* State = GetPlayerState<AAetheriaPlayerState>()) if (!State->TrySpendStamina(Cost)) Movement->bWantsToSprint = false; }
    }
    else SprintDrain = 0;
}
void AAetheriaCharacter::UpdateAvatar()
{
    const AAetheriaPlayerState* State = GetPlayerState<AAetheriaPlayerState>(); if (!State) return;
    const FName Avatar = State->GetPublicProfile().Avatar;
    const bool bMale = Avatar == TEXT("male");
    USkeletalMesh* Mesh = bMale ? MaleMesh.Get() : FemaleMesh.Get();
    if (!Mesh) return; // Source FBX must first be imported and assigned in the editor.
    if (VisibleAvatar != Avatar)
    {
        VisibleAvatar = Avatar; VisibleAnimation = nullptr;
        GetMesh()->SetSkeletalMeshAsset(Mesh); GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
        GetMesh()->SetRelativeRotation(FRotator(0, -90, 0)); EngineeringBody->SetVisibility(false);
    }
    const float Speed = GetVelocity().Size2D();
    UAnimSequence* Animation = bMale ? (Speed > 520 ? MaleRun.Get() : Speed > 30 ? MaleWalk.Get() : MaleIdle.Get()) : (Speed > 520 ? FemaleRun.Get() : Speed > 30 ? FemaleWalk.Get() : FemaleIdle.Get());
    if (Animation && Animation != VisibleAnimation) { VisibleAnimation = Animation; GetMesh()->PlayAnimation(Animation, true); }
}
void AAetheriaCharacter::SprintPressed() { if (CanAct()) CastChecked<UAetheriaCharacterMovement>(GetCharacterMovement())->bWantsToSprint = true; }
void AAetheriaCharacter::SprintReleased() { CastChecked<UAetheriaCharacterMovement>(GetCharacterMovement())->bWantsToSprint = false; }
AActor* AAetheriaCharacter::GetFocusedInteractable() const
{
    AActor* Best = nullptr; double Distance = FMath::Square(425.f);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->GetClass()->ImplementsInterface(UAetheriaInteractable::StaticClass()))
        {
            const double Current = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
            if (Current < Distance) { Distance = Current; Best = *It; }
        }
    return Best;
}
void AAetheriaCharacter::PerformInteract() { if (CanAct()) if (AAetheriaPlayerController* Player = Cast<AAetheriaPlayerController>(GetController())) Player->ServerInteract(GetFocusedInteractable()); }
void AAetheriaCharacter::MapPressed() { if (CanAct()) if (AAetheriaPlayerController* Player = Cast<AAetheriaPlayerController>(GetController())) Player->OpenCityMap(); }
void AAetheriaCharacter::DodgePressed() { if (CanAct()) ServerDodge(); }
void AAetheriaCharacter::ServerDodge_Implementation()
{
    AAetheriaPlayerState* State = GetPlayerState<AAetheriaPlayerState>(); const double Now = GetWorld()->GetTimeSeconds();
    if (!CanAct() || !State || Now - LastDodge < 1.2 || !State->TrySpendStamina(20)) return;
    LastDodge = Now; LaunchCharacter(GetActorForwardVector() * 850 + FVector(0, 0, 60), true, true);
    if (AAetheriaWorldRuntime* World = Cast<AAetheriaWorldRuntime>(UGameplayStatics::GetActorOfClass(this, AAetheriaWorldRuntime::StaticClass())))
        if (World->IsInTrialArea(GetActorLocation())) State->ApplyStoryEvent(Aetheria::Rules::StoryEvent::TrialDodge);
}
void AAetheriaCharacter::TargetPressed()
{
    for (TActorIterator<AAetheriaGuardian> It(GetWorld()); It; ++It)
        if (FVector::DistSquared(GetActorLocation(), It->GetActorLocation()) < FMath::Square(2500.f))
        { LockedTarget = *It; if (Controller) Controller->SetControlRotation((It->GetActorLocation() - GetActorLocation()).Rotation()); return; }
    LockedTarget.Reset();
}
void AAetheriaCharacter::CastRequested()
{
    if (!CanAct() || !Controller) return;
    FVector Aim = Controller->GetControlRotation().Vector();
    if (LockedTarget.IsValid()) Aim = (LockedTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    ServerCastRequested(TEXT("Primary"), Aim);
}
void AAetheriaCharacter::ServerCastRequested_Implementation(FName Id, FVector_NetQuantizeNormal Aim)
{
    const double Now = GetWorld()->GetTimeSeconds(); AAetheriaPlayerState* State = GetPlayerState<AAetheriaPlayerState>();
    if (!CanAct() || !State || Now - LastCastIntent < .15 || Aim.ContainsNaN() || Aim.SizeSquared() < .5 || !Controller) return;
    LastCastIntent = Now; const FVector Direction = Aim.GetSafeNormal();
    if (FVector::DotProduct(Direction, Controller->GetControlRotation().Vector()) < .2f) return;
    Aetheria::Rules::SpellSpec Spec; if (!State->ResolveSpell(Id, Spec)) return;
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(AetheriaCastOrigin), false, this);
    const FVector Origin = GetActorLocation() + FVector(0, 0, 45); const FVector Spawn = Origin + Direction * 90;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Spawn, ECC_Visibility, Params)) return;
    const Aetheria::Rules::Result Approved = State->TryCast(Spec);
    if (!Approved.ok) { if (AAetheriaPlayerController* Player = Cast<AAetheriaPlayerController>(Controller)) Player->ClientFeedback(UTF8_TO_TCHAR(Approved.message.c_str())); return; }
    if (Approved.healing > 0) return;
    FActorSpawnParameters SpawnParameters; SpawnParameters.Owner = this; SpawnParameters.Instigator = this;
    if (AAetheriaProjectile* Projectile = GetWorld()->SpawnActor<AAetheriaProjectile>(Spawn, Direction.Rotation(), SpawnParameters)) Projectile->InitializeServer(this, Approved, Spec, Direction);
}
