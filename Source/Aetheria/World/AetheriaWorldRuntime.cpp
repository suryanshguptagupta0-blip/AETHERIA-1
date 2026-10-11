#include "World/AetheriaWorldRuntime.h"
#include "Engine/StaticMesh.h"
#include "World/AetheriaFacilityActor.h"
#include "World/AetheriaCrystalActor.h"
#include "World/AetheriaTrialTarget.h"
#include "NPC/AetheriaNpcActor.h"
#include "AI/AetheriaGuardian.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
AAetheriaWorldRuntime::AAetheriaWorldRuntime()
{
    bReplicates = true; bAlwaysRelevant = true; RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
void AAetheriaWorldRuntime::BeginPlay() { Super::BeginPlay(); InitializeRuntime(); }
UHierarchicalInstancedStaticMeshComponent* AAetheriaWorldRuntime::Cell(FName Name, FLinearColor Color)
{
    UHierarchicalInstancedStaticMeshComponent* Result = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
    Result->SetupAttachment(RootComponent); Result->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Result->SetCollisionProfileName(TEXT("BlockAll")); Result->SetMobility(EComponentMobility::Static); Result->RegisterComponent();
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    { UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this); Material->SetVectorParameterValue(TEXT("Color"), Color); Result->SetMaterial(0, Material); }
    Cells.Add(Result); return Result;
}
void AAetheriaWorldRuntime::Cube(UHierarchicalInstancedStaticMeshComponent* Mesh, FVector Center, FVector Size)
{
    Mesh->AddInstance(FTransform(FQuat::Identity, Center, Size / 100.f));
}
void AAetheriaWorldRuntime::Label(const FString& Name, FVector Point)
{
    UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this); Text->SetupAttachment(RootComponent);
    Text->SetRelativeLocation(Point); Text->SetRelativeRotation(FRotator(0, -90, 0)); Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetText(FText::FromString(Name)); Text->SetTextRenderColor(FColor(220, 205, 150)); Text->SetWorldSize(55); Text->RegisterComponent();
    AddInstanceComponent(Text);
}
void AAetheriaWorldRuntime::Building(FName Id, const FString& Name, FVector Center, int32 Layout)
{
    // One HISM batch per building, so visibility bounds do not cover the entire city.
    UHierarchicalInstancedStaticMeshComponent* Mesh = Cell(Id, FLinearColor(.37, .32, .26));
    Cube(Mesh, Center + FVector(0, 0, -25), FVector(1600, 1400, 50));
    Cube(Mesh, Center + FVector(-800, 0, 175), FVector(40, 1400, 350));
    Cube(Mesh, Center + FVector(800, 0, 175), FVector(40, 1400, 350));
    Cube(Mesh, Center + FVector(0, 700, 175), FVector(1600, 40, 350));
    // A 300cm door opening. Upper lintel preserves the 340cm internal ceiling.
    Cube(Mesh, Center + FVector(-475, -700, 175), FVector(650, 40, 350));
    Cube(Mesh, Center + FVector(475, -700, 175), FVector(650, 40, 350));
    Cube(Mesh, Center + FVector(0, -700, 325), FVector(300, 40, 50));
    Cube(Mesh, Center + FVector(0, 0, 370), FVector(1640, 1440, 40));
    // Room-specific furniture, leaving a broad central walking corridor.
    const int32 Count = Layout == 0 || Layout == 1 ? 4 : Layout == 3 ? 3 : 2;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Offset(Index % 2 ? 520 : -520, -220 + (Index / 2) * 520, Layout == 1 ? 120 : 50);
        const FVector Size = Layout == 0 ? FVector(170, 290, 85) : Layout == 1 ? FVector(90, 260, 240) : FVector(240, 120, 90);
        Cube(Mesh, Center + Offset, Size);
    }
    if (Layout == 3) { Cube(Mesh, Center + FVector(430, 360, 155), FVector(30, 640, 310)); Cube(Mesh, Center + FVector(-480, 360, 60), FVector(240, 300, 100)); }
    Label(Name, Center + FVector(0, -750, 300));
    if (HasAuthority())
    {
        AAetheriaFacilityActor* Facility = GetWorld()->SpawnActor<AAetheriaFacilityActor>(Center + FVector(0, 0, 120), FRotator::ZeroRotator);
        if (Facility) Facility->FacilityId = Id;
    }
}
void AAetheriaWorldRuntime::InitializeRuntime()
{
    if (bInitialized || !GetWorld()) return; bInitialized = true;
    UHierarchicalInstancedStaticMeshComponent* Ground = Cell(TEXT("Ground"), FLinearColor(.15, .24, .18));
    Cube(Ground, FVector(2500, 0, -80), FVector(36000, 24000, 100));
    UHierarchicalInstancedStaticMeshComponent* Road = Cell(TEXT("Avenues"), FLinearColor(.24, .24, .25));
    Cube(Road, FVector(3000, 0, -12), FVector(22000, 800, 25));
    Cube(Road, FVector(-3000, 0, -12), FVector(650, 11000, 25)); Cube(Road, FVector(5200, 0, -12), FVector(650, 11000, 25));
    Building(TEXT("Hospital"), TEXT("Sanctum Hospital"), FVector(-5000, -3200, 0), 0);
    Building(TEXT("Hall"), TEXT("Awakening Hall"), FVector(-1600, -3500, 0), 2);
    Building(TEXT("Academy"), TEXT("Grand Arcane Lyceum"), FVector(2000, -3200, 0), 1);
    Building(TEXT("Guild"), TEXT("Common Guild Hall"), FVector(-4500, 3200, 0), 2);
    Building(TEXT("Houses"), TEXT("Residential Ward"), FVector(-1000, 3100, 0), 3);
    Building(TEXT("Market"), TEXT("Arcane Market"), FVector(2500, 3100, 0), 2);
    Building(TEXT("Bank"), TEXT("Aetheria Bank"), FVector(6200, -3500, 0), 2);
    Building(TEXT("Inn"), TEXT("The Lantern Inn"), FVector(6200, 3300, 0), 3);
    Building(TEXT("Library"), TEXT("Infinite Atheneum Library"), FVector(-5100, -6600, 0), 1);
    Label(TEXT("Magic Trial: 3 target hits + dodge + element interaction"), FVector(5200, -1000, 270));
    Label(TEXT("Fracture Guardian • Two phases"), FVector(10300, -1600, 300));
    if (GetNetMode() != NM_DedicatedServer && !UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()))
    {
        ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 1500), FRotator(-45, -35, 0));
        if (Sun) { Sun->GetLightComponent()->SetIntensity(3); }
        ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(); if (Sky) { Sky->GetLightComponent()->SetIntensity(.8f); }
    }
    for (int32 Index = 0; Index < 4; ++Index) Label(Index == 3 ? TEXT("Element focus") : TEXT("Practice target"), FVector(4500 + Index * 470, 450, 300));
    if (!HasAuthority()) return;
    const FName Ids[] = {TEXT("DoctorMira"), TEXT("ElderSeraphine"), TEXT("TutorElian"), TEXT("RegistrarVera"), TEXT("ResidentArden"), TEXT("MerchantNia"), TEXT("TellerOrin"), TEXT("InnkeeperSola")};
    const FVector Positions[] = {{-5300,-3300,110},{-1850,-3400,110},{1700,-3300,110},{-4800,3100,110},{-1300,3000,110},{2200,3000,110},{5900,-3600,110},{5900,3200,110}};
    for (int32 Index = 0; Index < 8; ++Index) { AAetheriaNpcActor* Npc = GetWorld()->SpawnActor<AAetheriaNpcActor>(Positions[Index], FRotator(0, -90, 0)); if (Npc) Npc->NpcId = Ids[Index]; }
    GetWorld()->SpawnActor<AAetheriaCrystalActor>(FVector(-1350, -3600, 120), FRotator::ZeroRotator);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        AAetheriaTrialTarget* Target = GetWorld()->SpawnActor<AAetheriaTrialTarget>(FVector(4500 + Index * 470, 450, 110), FRotator::ZeroRotator);
        if (Target) Target->bElementInteraction = Index == 3;
    }
    Guardian = GetWorld()->SpawnActor<AAetheriaGuardian>(FVector(11200, 0, 110), FRotator::ZeroRotator);

}
bool AAetheriaWorldRuntime::IsInTrialArea(FVector Point) const { return Point.X >= 3800 && Point.X <= 6600 && FMath::Abs(Point.Y) < 1100; }
FName AAetheriaWorldRuntime::GetFacilityAtLocation(FVector Point) const
{
    for (TActorIterator<AAetheriaFacilityActor> It(GetWorld()); It; ++It) if (It->Contains(Point)) return It->FacilityId;
    return NAME_None;
}
