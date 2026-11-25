#include "Motion/NanamiAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Combat/CombatComponent.h"
#include "Combat/NanamiFormComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// 动画实例重建时重新查找所属角色的形态组件。
void UNanamiAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    AActor* Owner = GetOwningActor();
    Form = Owner ? Owner->FindComponentByClass<UNanamiFormComponent>() : nullptr;
    bPlayingRunStop = false;
    ActiveRunStop = nullptr;
    RunStopTime = 0.0f;
    LastDrivenGroundSpeed = 0.0f;
    LastHoldInstanceId = 0;
    LastHoldSection = NAME_None;
}

// 将平面速度、相对移动角和形态写入该帧的并行动画图输入。
void UNanamiAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
    const FVector Velocity = Character ? Character->GetVelocity() : FVector::ZeroVector;
    const float Speed = Velocity.Size2D();
    const UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    const bool bCanLocomote = Movement && Movement->IsMovingOnGround()
        && (!Combat || Combat->GetActiveInstanceId() == 0);
    const bool bIsLocomoting = bCanLocomote && Speed > 5.0f;
    GroundSpeed = bIsLocomoting ? Speed : 0.0f;
    MoveDirection = bIsLocomoting
        ? FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Velocity, Character->GetActorRightVector()),
                                              FVector::DotProduct(Velocity, Character->GetActorForwardVector())))
        : 0.0f;

    if (!Form.IsValid())
    {
        // 蓝图创建的组件可能晚于原生 Mesh 注册，首个有效角色帧再绑定。
        if (AActor* Owner = GetOwningActor())
        {
            Form = Owner->FindComponentByClass<UNanamiFormComponent>();
        }
    }
    // 焚冲的持斧姿势由蒙太奇提供，松手、耗尽和自然结束都从同一个剑盾待机混出。
    if (Form.IsValid() && Form->GetYellowHoldInstanceId() != 0)
    {
        bShieldAxe = false;
    }
    else
    {
        // 普通切形刚混入时仍看得到旧待机，等动作盖住它再换成目标待机。
        const FAnimMontageInstance* Instance = GetActiveMontageInstance();
        if (!Instance || Instance->GetDesiredWeight() <= Instance->GetWeight() + KINDA_SMALL_NUMBER)
        {
            bShieldAxe = Form.IsValid() && Form->GetWeaponForm() == ENanamiWeaponForm::ShieldAxe;
        }
    }

    const bool bHasMovementInput = bCanLocomote && !Movement->GetCurrentAcceleration().IsNearlyZero();
    UpdateRunStop(DeltaSeconds, bCanLocomote, bHasMovementInput);
}

// 用片段自身的时长采样停步，混出期间保留最后的采样位置。
void UNanamiAnimInstance::UpdateRunStop(float DeltaSeconds, bool bCanLocomote, bool bHasMovementInput)
{
    if (!bCanLocomote || bHasMovementInput)
    {
        bPlayingRunStop = false;
    }
    else if (LastDrivenGroundSpeed >= RunStopMinSpeed)
    {
        ActiveRunStop = bShieldAxe ? AxeRunStop.Get() : SwordRunStop.Get();
        RunStopTime = 0.0f;
        bPlayingRunStop = ActiveRunStop && ActiveRunStop->RateScale > 0.0f && ActiveRunStop->GetPlayLength() > 0.0f;
    }
    else if (bPlayingRunStop)
    {
        const float PlayLength = ActiveRunStop->GetPlayLength();
        RunStopTime = FMath::Min(RunStopTime + DeltaSeconds * ActiveRunStop->RateScale, PlayLength);
        bPlayingRunStop = RunStopTime < PlayLength - UE_KINDA_SMALL_NUMBER;
    }
    LastDrivenGroundSpeed = bHasMovementInput ? GroundSpeed : 0.0f;
}

// 收势从任意攻击帧接入；保持 Montage 时间和胶囊根运动，仅衰减骨架姿势的突变。
void UNanamiAnimInstance::Montage_Advance(float DeltaSeconds)
{
    Super::Montage_Advance(DeltaSeconds);
    const int64 InstanceId = Form.IsValid() ? Form->GetYellowHoldInstanceId() : 0;
    const FAnimMontageInstance* Instance = InstanceId != 0 ? GetActiveMontageInstance() : nullptr;
    const FName Section = Instance ? Instance->GetCurrentSection() : NAME_None;
    if (Instance && InstanceId == LastHoldInstanceId && !LastHoldSection.IsNone()
        && Section != LastHoldSection && Form->IsYellowHoldExitSection(Section))
    {
        RequestMontageInertialization(Instance->Montage.Get(), ReleasePoseBlendSeconds);
    }
    LastHoldInstanceId = InstanceId;
    LastHoldSection = Section;
}
