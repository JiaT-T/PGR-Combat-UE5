#include "Motion/CombatMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Motion/CombatActionSet.h"

// 按动作表播放对应蒙太奇，并记录本次实例的结束回调和输入缓存窗口。
bool UCombatMotionComponent::PlayAction(FName ActionId, FOnMontageEnded OnEnded)
{
    const FCombatActionDefinition* Action = ActionSet ? ActionSet->Actions.Find(ActionId) : nullptr;
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Action || !Action->Montage || !AnimInstance)
    {
        return false;
    }
    if (!FMath::IsFinite(Action->PlayRate) || Action->PlayRate <= 0.0f)
    {
        return false;
    }

    UAnimMontage* Montage = Action->Montage;
    float Start = 0.0f;
    float End = Montage->GetPlayLength();
    if (!Action->Section.IsNone())
    {
        const int32 Index = Montage->GetSectionIndex(Action->Section);
        if (Index == INDEX_NONE)
        {
            return false;
        }
        Montage->GetSectionStartAndEndTime(Index, Start, End);
    }

    if (AnimInstance->Montage_Play(Montage, Action->PlayRate, EMontagePlayReturnType::MontageLength, Start, false) <= 0.0f)
    {
        return false;
    }

    FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
    if (!Instance)
    {
        return false;
    }
    if (!Action->Section.IsNone())
    {
        // 一个 Section 可以对应一招，同一 Montage 的下一段由新输入驱动。
        AnimInstance->Montage_SetNextSection(Action->Section, NAME_None, Montage);
    }

    Instance->OnMontageEnded = MoveTemp(OnEnded);
    PlayingAnimInstance = AnimInstance;
    MontageInstanceId = Instance->GetInstanceID();
    BufferEnd = End;
    BufferStart = FMath::Clamp(End - Action->InputBufferSeconds, Start, End);
    return true;
}

// 用当前蒙太奇实例的本地播放位置判断是否进入输入缓存窗口。
bool UCombatMotionComponent::IsInputBufferOpen() const
{
    UAnimInstance* AnimInstance = PlayingAnimInstance.Get();
    const FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(MontageInstanceId) : nullptr;
    return Instance && BufferStart < BufferEnd && Instance->GetPosition() >= BufferStart && Instance->GetPosition() <= BufferEnd;
}

// 按实例编号停止组件先前启动的蒙太奇并清除跟踪记录。
void UCombatMotionComponent::StopAction()
{
    UAnimInstance* AnimInstance = PlayingAnimInstance.Get();
    FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(MontageInstanceId) : nullptr;
    MontageInstanceId = INDEX_NONE;
    PlayingAnimInstance.Reset();
    if (Instance && Instance->IsActive())
    {
        Instance->Stop(FAlphaBlend(0.0f));
    }
}
