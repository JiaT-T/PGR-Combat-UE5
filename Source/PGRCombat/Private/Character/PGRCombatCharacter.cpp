#include "Character/PGRCombatCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatTypes.h"
#include "Combat/NanamiFormComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Motion/CombatActionSet.h"
#include "Motion/CombatMotionComponent.h"

// 创建战斗和动作组件，并设置胶囊体、模型原点与训练场镜头。
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

// 组件初始化完成后，将角色配置中的网格、动画类和动作表交给对应组件。
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
    SetAnimRootMotionTranslationScale(FighterRootMotionTranslationScale);

    if (MoveAction)
    {
        bUseControllerRotationYaw = false;
        UCharacterMovementComponent* Movement = GetCharacterMovement();
        Movement->bUseControllerDesiredRotation = false;
        Movement->bOrientRotationToMovement = true;
        Combat->OnActionChanged.AddUniqueDynamic(this, &ThisClass::OnCombatActionChanged);
    }
}

// 切换控制器或重新接管角色时，重新安装当前本地玩家的战斗输入映射。
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

// 从注册时的本地玩家移除战斗映射，并清除缓存的输入子系统。
void APGRCombatCharacter::RemoveCombatMapping()
{
    if (MoveAction)
    {
        ConsumeMovementInputVector();
    }
    if (Combat)
    {
        Combat->ClearBufferedInput();
        Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Canceled);
    }
    if (UNanamiFormComponent* Form = FindComponentByClass<UNanamiFormComponent>())
    {
        Form->CancelYellow();
    }
    // 输入映射挂在 LocalPlayer 上，失去控制时需要主动移除本角色的映射。
    if (InputSubsystem.IsValid() && CombatMapping)
    {
        InputSubsystem->RemoveMappingContext(CombatMapping);
    }
    InputSubsystem.Reset();
}

// 交还角色控制权前，解除本地玩家与本角色的输入映射。
void APGRCombatCharacter::UnPossessed()
{
    RemoveCombatMapping();
    Super::UnPossessed();
}

// 锁定期间暂停普通移动输入，完整收招后恢复常规控制。
bool APGRCombatCharacter::IsMoveInputIgnored() const
{
    return (Combat && Combat->IsActionLocked()) || Super::IsMoveInputIgnored();
}

// 角色退出世界前，清理仍在本地玩家上生效的映射。
void APGRCombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveCombatMapping();
    Super::EndPlay(EndPlayReason);
}

// 绑定持续移动与战斗指令，黄球单独接收按下、松开和取消。
void APGRCombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (Input && BasicAttackAction)
    {
        Input->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &ThisClass::OnBasicAttackPressed);
        Input->BindAction(BasicAttackAction, ETriggerEvent::Completed, this, &ThisClass::OnBasicAttackReleased);
        Input->BindAction(BasicAttackAction, ETriggerEvent::Canceled, this, &ThisClass::OnBasicAttackCanceled);
    }
    if (Input && BlueOrbAction)
    {
        Input->BindAction(BlueOrbAction, ETriggerEvent::Started, this, &ThisClass::OnBlueOrbPressed);
    }
    if (Input && RedOrbAction)
    {
        Input->BindAction(RedOrbAction, ETriggerEvent::Started, this, &ThisClass::OnRedOrbPressed);
    }
    if (Input && UltimateAction)
    {
        Input->BindAction(UltimateAction, ETriggerEvent::Started, this, &ThisClass::OnUltimatePressed);
    }
    if (Input && YellowOrbAction)
    {
        if (UNanamiFormComponent* Form = FindComponentByClass<UNanamiFormComponent>())
        {
            Input->BindAction(YellowOrbAction, ETriggerEvent::Started, Form, &UNanamiFormComponent::PressYellow);
            Input->BindAction(YellowOrbAction, ETriggerEvent::Completed, Form, &UNanamiFormComponent::ReleaseYellow);
            Input->BindAction(YellowOrbAction, ETriggerEvent::Canceled, Form, &UNanamiFormComponent::CancelYellow);
        }
    }
    // 同帧的出招与松键确认先于持续方向输入处理。
    if (Input && MoveAction)
    {
        Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::OnMove);
    }
}

// 每帧只提交本地玩家仍在按的方向，斜向输入保持与单轴相同的最大幅度。
void APGRCombatCharacter::OnMove(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
    if (Axis.IsNearlyZero() || Super::IsMoveInputIgnored()
        || (Combat->GetActiveInstanceId() != 0 && !Combat->TryCancelForMovement()))
    {
        return;
    }

    const FRotator ViewYaw(0.0, GetControlRotation().Yaw, 0.0);
    AddMovementInput(ViewYaw.RotateVector(FVector(Axis.Y, Axis.X, 0.0)));
}

// 仅在常态移动交给动作时清一次本帧输入和水平惯性，连段保留原有根运动衔接。
void APGRCombatCharacter::OnCombatActionChanged(FName, int64, ECombatActionEvent)
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    const bool bActionActive = Combat->GetActiveInstanceId() != 0;
    if (bActionActive && Movement->bOrientRotationToMovement)
    {
        // 同帧方向键可能先于动作键触发，丢掉已经提交但尚未消费的输入。
        ConsumeMovementInputVector();
        if (Movement->IsMovingOnGround())
        {
            Movement->StopMovementImmediately();
        }
        else
        {
            Movement->Velocity.X = 0.0;
            Movement->Velocity.Y = 0.0;
        }
    }
    Movement->bOrientRotationToMovement = !bActionActive;
}

// 按下必杀键时沿用统一的动作启动、打断与衔接入口。
void APGRCombatCharacter::OnUltimatePressed()
{
    Combat->SubmitInput(ECombatCommand::Ultimate, ECombatInputPhase::Pressed);
}

// 向战斗组件提交一次普攻按下指令。
void APGRCombatCharacter::OnBasicAttackPressed()
{
    Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed);
}

// 将普攻输入结束作为松开指令提交给战斗组件。
void APGRCombatCharacter::OnBasicAttackReleased()
{
    Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Released);
}

// 保留输入取消的语义，供脚本区分取消与正常松开。
void APGRCombatCharacter::OnBasicAttackCanceled()
{
    Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Canceled);
}

// 每次按下红球键立即提交一次输入。
void APGRCombatCharacter::OnRedOrbPressed()
{
    Combat->SubmitInput(ECombatCommand::RedOrb, ECombatInputPhase::Pressed);
}

// 按下当帧提交一次蓝球指令。
void APGRCombatCharacter::OnBlueOrbPressed()
{
    Combat->SubmitInput(ECombatCommand::BlueOrb, ECombatInputPhase::Pressed);
}
