#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AetheriaWorldRuntime.generated.h"
class UHierarchicalInstancedStaticMeshComponent; class AAetheriaGuardian;
UCLASS()
class AETHERIA_API AAetheriaWorldRuntime : public AActor
{
    GENERATED_BODY()
public:
    AAetheriaWorldRuntime();
    virtual void BeginPlay() override;
    void InitializeRuntime();
    FVector GetHospitalSpawn() const { return FVector(-5350, -3450, 110); }
    FRotator GetHospitalSpawnRotation() const { return FRotator(0, 90, 0); }
    bool IsInTrialArea(FVector Point) const;
    FName GetFacilityAtLocation(FVector Point) const;
private:
    bool bInitialized = false;
    UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Cells;
    UPROPERTY() TObjectPtr<AAetheriaGuardian> Guardian;
    UHierarchicalInstancedStaticMeshComponent* Cell(FName Name, FLinearColor Color);
    void Cube(UHierarchicalInstancedStaticMeshComponent* Mesh, FVector Center, FVector Size);
    void Building(FName Id, const FString& Name, FVector Center, int32 Layout);
    void Label(const FString& Name, FVector Point);
};
