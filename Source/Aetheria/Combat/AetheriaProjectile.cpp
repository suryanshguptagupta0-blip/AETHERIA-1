#include "Combat/AetheriaProjectile.h"
#include "Engine/StaticMesh.h"
#include "Core/AetheriaPlayerState.h"
#include "AI/AetheriaGuardian.h"
#include "World/AetheriaTrialTarget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
AAetheriaProjectile::AAetheriaProjectile()
{
    bReplicates = true; SetReplicateMovement(true); NetUpdateFrequency = 30;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); RootComponent = Collision;
    Collision->SetSphereRadius(14); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic")); Collision->SetNotifyRigidBodyCollision(true);
    Collision->OnComponentHit.AddDynamic(this, &AAetheriaProjectile::Hit);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); if (Mesh.Succeeded()) Visual->SetStaticMesh(Mesh.Object);
    Visual->SetRelativeScale3D(FVector(.28)); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement")); Movement->SetUpdatedComponent(Collision);
    Movement->bRotationFollowsVelocity = true; Movement->ProjectileGravityScale = 0; Movement->bShouldBounce = false;
}
void AAetheriaProjectile::BeginPlay() { Super::BeginPlay(); if (!HasAuthority()) Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
void AAetheriaProjectile::InitializeServer(APawn* Caster, const Aetheria::Rules::Result& Approved, const Aetheria::Rules::SpellSpec& Spec, FVector Direction)
{
    if (!HasAuthority() || !Caster || !Approved.ok || !Approved.castId) { Destroy(); return; }
    SetInstigator(Caster); SetOwner(Caster); CastId = Approved.castId; Collision->IgnoreActorWhenMoving(Caster, true);
    Movement->InitialSpeed = Spec.speed * 100; Movement->MaxSpeed = Movement->InitialSpeed;
    Movement->Velocity = Direction.GetSafeNormal() * Movement->InitialSpeed;
    SetLifeSpan(FMath::Clamp(Spec.range / Spec.speed, .1f, 24.f));
}
void AAetheriaProjectile::Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Result)
{
    if (!HasAuthority() || bResolved || !Other || Other == GetInstigator()) return;
    bResolved = true;
    AAetheriaPlayerState* State = GetInstigator() ? GetInstigator()->GetPlayerState<AAetheriaPlayerState>() : nullptr;
    if (AAetheriaGuardian* Guardian = Cast<AAetheriaGuardian>(Other)) Guardian->ReceiveCast(State, CastId);
    else if (AAetheriaTrialTarget* Target = Cast<AAetheriaTrialTarget>(Other)) Target->ReceiveCast(State, CastId);
    Destroy();
}
