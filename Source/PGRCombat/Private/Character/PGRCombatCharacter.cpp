#include "Character/PGRCombatCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Motion/CombatActionSet.h"
#include "Motion/CombatMotionComponent.h"

APGRCombatCharacter::APGRCombatCharacter()
{
    Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
    Motion = CreateDefaultSubobject<UCombatMotionComponent>(TEXT("Motion"));
    GetCapsuleComponent()->InitCapsuleSize(38.0f, 88.0f);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    auto* CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
    CameraArm->SetupAttachment(RootComponent);
    CameraArm->SetRelativeRotation(FRotator(-10.0f, 145.0f, 0.0f));
    CameraArm->TargetArmLength = 380.0f;
    auto* Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
    Camera->FieldOfView = 50.0f;
}

void APGRCombatCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (FighterMesh)
    {
        GetMesh()->SetSkeletalMeshAsset(FighterMesh);
    }
    if (FighterAnimClass)
    {
        GetMesh()->SetAnimInstanceClass(FighterAnimClass);
    }
    Motion->ActionSet = ActionSet;
}

void APGRCombatCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    RemoveCombatMapping();
    const auto* Player = Cast<APlayerController>(Controller);
    if (Player && Player->GetLocalPlayer() && CombatMapping)
    {
        InputSubsystem = Player->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        if (InputSubsystem.IsValid())
        {
            InputSubsystem->AddMappingContext(CombatMapping, 0);
        }
    }
}

void APGRCombatCharacter::RemoveCombatMapping()
{
    // 输入映射挂在 LocalPlayer 上，失去控制时需要主动移除本角色的映射。
    if (InputSubsystem.IsValid() && CombatMapping)
    {
        InputSubsystem->RemoveMappingContext(CombatMapping);
    }
    InputSubsystem.Reset();
}

void APGRCombatCharacter::UnPossessed()
{
    RemoveCombatMapping();
    Super::UnPossessed();
}

void APGRCombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveCombatMapping();
    Super::EndPlay(EndPlayReason);
}

void APGRCombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (Input && BasicAttackAction)
    {
        Input->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &ThisClass::OnBasicAttackPressed);
        Input->BindAction(BasicAttackAction, ETriggerEvent::Completed, this, &ThisClass::OnBasicAttackReleased);
        Input->BindAction(BasicAttackAction, ETriggerEvent::Canceled, this, &ThisClass::OnBasicAttackReleased);
    }
}

void APGRCombatCharacter::OnBasicAttackPressed()
{
    Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed);
}

void APGRCombatCharacter::OnBasicAttackReleased()
{
    Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Released);
}
