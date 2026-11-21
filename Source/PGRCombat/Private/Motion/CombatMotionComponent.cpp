#include "Motion/CombatMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Motion/CombatActionSet.h"

// 播放选定的蒙太奇，成功后用新段的混入时长淡出先前的动作。
bool UCombatMotionComponent::PlayAction(FName ActionId, ECombatCommand Command, FOnMontageEnded OnEnded)
{
    const FCombatActionDefinition* Action = ActionSet ? ActionSet->Actions.Find(ActionId) : nullptr;
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Action || !Action->Montage || !AnimInstance)
    {
        return false;
    }
    if (Action->Command != Command)
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
    UAnimInstance* PreviousAnimInstance = PlayingAnimInstance.Get();
    const int32 PreviousInstanceId = MontageInstanceId;
    PlayingAnimInstance = AnimInstance;
    MontageInstanceId = Instance->GetInstanceID();
    BufferEnd = End;
    BufferStart = FMath::Clamp(End - Action->InputBufferSeconds, Start, End);
    if (PreviousAnimInstance)
    {
        FAnimMontageInstance* Previous = PreviousAnimInstance->GetMontageInstanceForID(PreviousInstanceId);
        if (Previous && Previous->IsActive())
        {
            Previous->Stop(Montage->BlendIn);
        }
    }
    return true;
}

// 用当前蒙太奇实例的本地播放位置判断是否允许立即续段。
bool UCombatMotionComponent::IsInputBufferOpen() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance();
    return Instance && BufferStart < BufferEnd && Instance->GetPosition() >= BufferStart && Instance->GetPosition() <= BufferEnd;
}

// 按已记录的实例编号定位动画，拒绝访问已结束的实例。
FAnimMontageInstance* UCombatMotionComponent::GetPlayingInstance() const
{
    UAnimInstance* Anim = PlayingAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(MontageInstanceId) : nullptr;
    return Instance && Instance->IsActive() ? Instance : nullptr;
}

// 获取当前实际播放的 Section，供武器和动作状态同步。
FName UCombatMotionComponent::GetCurrentActionSection() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance();
    return Instance ? Instance->GetCurrentSection() : NAME_None;
}

// 对照目标段的实际时间，避免写死动画时长或重播已进入的收刀。
bool UCombatMotionComponent::IsBeforeActionSection(FName Section) const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance();
    const UAnimMontage* Montage = Instance ? Instance->Montage.Get() : nullptr;
    const int32 Index = Montage ? Montage->GetSectionIndex(Section) : INDEX_NONE;
    return Index != INDEX_NONE && Instance->GetPosition() < Montage->GetAnimCompositeSection(Index).GetTime();
}

// 中途松手时直接移动本实例的播放游标，保留原有结束回调。
bool UCombatMotionComponent::JumpForwardToActionSection(FName Section)
{
    FAnimMontageInstance* Instance = GetPlayingInstance();
    return Instance && IsBeforeActionSection(Section) && Instance->JumpToSectionName(Section);
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
