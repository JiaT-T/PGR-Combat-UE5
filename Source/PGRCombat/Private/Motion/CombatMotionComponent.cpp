#include "Motion/CombatMotionComponent.h"
#include "NanamiLocalVfxComponent.h"

#include "Animation/AnimInstance.h"
#include "Combat/NanamiFormComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Motion/CombatActionSet.h"

namespace
{
    // 盾斧优先选用当前动作的形态变体，未配置变体的动作沿用默认 Ability。
    const UCombatMontageAbility* FindAbility(const UCombatActionSet& Set, FName ActionId, const ACharacter& Character)
    {
        const auto* Form = Character.FindComponentByClass<UNanamiFormComponent>();
        if (Form && Form->GetWeaponForm() == ENanamiWeaponForm::ShieldAxe)
        {
            if (const auto* Variant = Set.ShieldAxeAbilities.Find(ActionId))
            {
                return Variant->GetDefaultObject();
            }
        }
        const auto* Default = Set.Abilities.Find(ActionId);
        return Default ? Default->GetDefaultObject() : nullptr;
    }

    // 按当前播放位置区分提前按下和即按即切，关窗后的新按键直接忽略。
    ECombatChainWindow ReadChainWindow(const FAnimMontageInstance* Instance,
        const FCombatAbilityWindow& Buffer, const FCombatAbilityWindow& Chain, float StartTolerance = 0.f)
    {
        if (!Instance)
        {
            return ECombatChainWindow::Closed;
        }
        const float Position = Instance->GetPosition();
        if (Chain.Contains(Position, StartTolerance))
        {
            return ECombatChainWindow::Chain;
        }
        if (Position < Chain.StartSeconds && Buffer.Contains(Position, StartTolerance))
        {
            return ECombatChainWindow::Buffer;
        }
        return ECombatChainWindow::Closed;
    }
}

// 播放选定的蒙太奇，成功后用新段的混入时长淡出先前的动作。
bool UCombatMotionComponent::PlayAction(FName ActionId, ECombatCommand Command, FOnMontageEnded OnEnded)
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance || !ActionSet)
    {
        return false;
    }
    const UCombatMontageAbility* Ability = FindAbility(*ActionSet, ActionId, *Character);
    if (!Ability || Ability->Command != Command)
    {
        return false;
    }
    FCombatMontageTasks Tasks;
    if (!Ability->ResolveTasks(Tasks))
    {
        return false;
    }

    // 重播同一个 Montage 也会替换旧实例，先从旧游标判定组次，成功起播后再应用。
    const bool bSecondDodge = Tasks.DodgePairRestartBuffer.EndSeconds > 0.f && CanChainSecondDodge();
    UAnimMontage* Montage = Tasks.Montage;
    if (AnimInstance->Montage_Play(Montage, Tasks.PlayRate, EMontagePlayReturnType::MontageLength, Tasks.StartPosition, false) <= 0.0f)
    {
        return false;
    }

    FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
    if (!Instance)
    {
        return false;
    }
    if (!Tasks.Section.IsNone())
    {
        // 一个 Section 可以对应一招，同一 Montage 的下一段由新输入驱动。
        AnimInstance->Montage_SetNextSection(Tasks.Section, NAME_None, Montage);
    }

    Instance->OnMontageEnded = MoveTemp(OnEnded);
    UAnimInstance* PreviousAnimInstance = PlayingAnimInstance.Get();
    const int32 PreviousInstanceId = MontageInstanceId;
    PlayingAnimInstance = AnimInstance;
    MontageInstanceId = Instance->GetInstanceID();
    UNanamiLocalVfxComponent::TrackMontage(*Character->GetMesh(), *AnimInstance, *Montage, ActionId, MontageInstanceId, Tasks.StartPosition);
    bBlendingOutNaturally = false;
    // UE 在自然淡出开始时就把实例标为非活动，此时收势窗口仍可能接受普攻。
    Instance->OnMontageBlendingOutStarted.BindWeakLambda(this, [this, Id = MontageInstanceId](UAnimMontage*, bool bInterrupted)
    {
        if (MontageInstanceId == Id)
        {
            bBlendingOutNaturally = !bInterrupted;
        }
    });
    ExitSection = Tasks.ExitSection;
    bRequireFullPlayback = Tasks.bRequireFullPlayback;
    InputBuffer = Tasks.InputBuffer;
    ChainWindow = Tasks.ChainWindow;
    bAllowChainDuringNaturalBlendOut = Tasks.bAllowChainDuringNaturalBlendOut;
    MoveCancelWindow = Tasks.MoveCancelWindow;
    MoveBlendOutSeconds = Tasks.MoveBlendOutSeconds;
    BasicFollowUpBuffer = Tasks.BasicFollowUpBuffer;
    BasicFollowUpChain = Tasks.BasicFollowUpChain;
    YellowTapBuffer = Tasks.YellowTapBuffer;
    YellowTapChain = Tasks.YellowTapChain;
    DodgeBuffer = Tasks.DodgeBuffer;
    DodgeChain = Tasks.DodgeChain;
    DodgePairRestartPosition = Tasks.DodgePairRestartBuffer.EndSeconds;
    if (bSecondDodge)
    {
        // 第二闪只接受重开窗口；下次成功起闪将按新一组的第一闪处理。
        DodgeBuffer = Tasks.DodgePairRestartBuffer;
        DodgeChain.StartSeconds = DodgePairRestartPosition;
    }
    UltimateBuffer = Tasks.UltimateBuffer;
    UltimateChain = Tasks.UltimateChain;
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

// 先检查直接续段，再接收早按；经过最后一个窗口后的新输入始终被拒绝。
ECombatChainWindow UCombatMotionComponent::GetChainWindow(bool bYellowTap) const
{
    if (UsesYellowTapWindow(bYellowTap))
    {
        return ReadChainWindow(GetPlayingInstance(true), YellowTapBuffer, YellowTapChain);
    }
    return ReadChainWindow(GetPlayingInstance(bAllowChainDuringNaturalBlendOut), InputBuffer, ChainWindow);
}

// 移动取消可以发生在自然淡出，仍按同一个 Montage 实例的游标判断。
bool UCombatMotionComponent::IsMoveCancelWindowOpen() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance(true);
    return Instance && MoveCancelWindow.Contains(Instance->GetPosition());
}

// 允许普攻专用窗口延伸至本次 Montage 的自然淡出。
ECombatChainWindow UCombatMotionComponent::GetBasicFollowUpWindow() const
{
    return ReadChainWindow(GetPlayingInstance(true), BasicFollowUpBuffer, BasicFollowUpChain);
}

// 新闪避使用来源 Ability 的窗口，按停止的旧实例不能重获资格。
ECombatChainWindow UCombatMotionComponent::GetDodgeWindow() const
{
    return ReadChainWindow(GetPlayingInstance(true), DodgeBuffer, DodgeChain, GetDodgeWindowTolerance());
}

// 判定上一闪是否仍允许组内快接，跨过重开点的独立按键会开启新的一组。
bool UCombatMotionComponent::CanChainSecondDodge() const
{
    if (DodgeChain.StartSeconds >= DodgePairRestartPosition)
    {
        return false;
    }
    const FAnimMontageInstance* Instance = GetPlayingInstance(true);
    return Instance && DodgeChain.Contains(Instance->GetPosition(), UE_KINDA_SMALL_NUMBER)
        && Instance->GetPosition() < DodgePairRestartPosition - UE_KINDA_SMALL_NUMBER;
}

// 允许已缓存的闪避在动画实际经过开窗点时起播。
bool UCombatMotionComponent::HasReachedDodgeWindow() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance(true);
    return Instance && DodgeChain.StartSeconds < DodgeChain.EndSeconds
        && Instance->GetPosition() + GetDodgeWindowTolerance() >= DodgeChain.StartSeconds;
}

// 按本次 Montage 的实际游标判断必杀窗口，也支持跳入焚冲收势段。
ECombatChainWindow UCombatMotionComponent::GetUltimateWindow() const
{
    return ReadChainWindow(GetPlayingInstance(true), UltimateBuffer, UltimateChain);
}

// 允许已缓存的按键在两次更新之间跨过开窗点。
bool UCombatMotionComponent::HasReachedUltimateWindow() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance(true);
    return HasUltimateWindow() && Instance && Instance->GetPosition() >= UltimateChain.StartSeconds;
}

// 已接受的普攻输入在跨帧经过切招点时仍然有效。
bool UCombatMotionComponent::HasReachedBasicFollowUpWindow() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance(true);
    return HasBasicFollowUp() && Instance && Instance->GetPosition() >= BasicFollowUpChain.StartSeconds;
}

// 缓存只会在切招起点之前建立，跨帧跳过窄窗口时仍能消费已按下的输入。
bool UCombatMotionComponent::HasReachedChainWindow(bool bYellowTap) const
{
    const bool bUseTapWindow = UsesYellowTapWindow(bYellowTap);
    const FAnimMontageInstance* Instance = GetPlayingInstance(bUseTapWindow || bAllowChainDuringNaturalBlendOut);
    const FCombatAbilityWindow& Chain = bUseTapWindow ? YellowTapChain : ChainWindow;
    return Instance && Instance->GetPosition() >= Chain.StartSeconds;
}

// 仅当本段还会经过衔接窗口时提前排入下一招。
bool UCombatMotionComponent::IsBeforeChainWindow() const
{
    const FAnimMontageInstance* Instance = GetPlayingInstance(bAllowChainDuringNaturalBlendOut);
    return Instance && ChainWindow.StartSeconds < ChainWindow.EndSeconds && Instance->GetPosition() < ChainWindow.StartSeconds;
}

// 按已记录的实例编号定位动画，拒绝访问已结束的实例。
FAnimMontageInstance* UCombatMotionComponent::GetPlayingInstance(bool bIncludeNaturalBlendOut) const
{
    UAnimInstance* Anim = PlayingAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(MontageInstanceId) : nullptr;
    return Instance && Instance->IsValid() && (!Instance->IsStopped() || (bIncludeNaturalBlendOut && bBlendingOutNaturally))
        ? Instance : nullptr;
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
void UCombatMotionComponent::StopAction(float BlendOutSeconds)
{
    FAnimMontageInstance* Instance = GetPlayingInstance(true);
    MontageInstanceId = INDEX_NONE;
    ExitSection = NAME_None;
    MoveCancelWindow = {};
    MoveBlendOutSeconds = 0.f;
    BasicFollowUpBuffer = {};
    BasicFollowUpChain = {};
    YellowTapBuffer = {};
    YellowTapChain = {};
    DodgeBuffer = {};
    DodgeChain = {};
    DodgePairRestartPosition = 0.f;
    UltimateBuffer = {};
    UltimateChain = {};
    bBlendingOutNaturally = false;
    PlayingAnimInstance.Reset();
    if (Instance)
    {
        if (BlendOutSeconds > 0.f)
        {
            // 淡出的旧实例只贡献姿势，胶囊从开窗当帧起由角色移动组件驱动。
            Instance->PushDisableRootMotion();
        }
        FAlphaBlend BlendOut(BlendOutSeconds);
        BlendOut.SetBlendOption(EAlphaBlendOption::CubicInOut);
        Instance->Stop(BlendOut);
    }
}
