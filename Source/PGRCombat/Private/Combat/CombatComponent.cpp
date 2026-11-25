#include "Combat/CombatComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Motion/CombatMotionComponent.h"
#include "Scripting/CombatScriptSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRCombat, Log, All);

namespace
{
    // 技能球共享衔接窗口，并可在确认发动时打断当前普攻。
    bool IsSignalOrb(ECombatCommand Command)
    {
        return Command == ECombatCommand::RedOrb || Command == ECombatCommand::BlueOrb || Command == ECombatCommand::YellowOrb;
    }
}

// 平时关闭 Tick，仅在缓冲与实际可切招时刻之间跟随动画进度。
UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

// 接受战斗输入前，从同一角色取得动作播放组件。
void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    bEndingPlay = false;
    Motion = GetOwner()->FindComponentByClass<UCombatMotionComponent>();
}

// 关闭新的输入受理，并在组件离开世界前打断当前动作。
void UCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    ResetBasicPress();
    InterruptActiveAction();
    Super::EndPlay(Reason);
}

// 按脚本选招，同类动作检查接续窗口，跨指令输入交给打断规则。
ECombatInputResult UCombatComponent::SubmitInput(ECombatCommand Command, ECombatInputPhase Phase)
{
    UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UCombatScriptSubsystem* Scripts = Game ? Game->GetSubsystem<UCombatScriptSubsystem>() : nullptr;
    if (bEndingPlay || !Scripts || !Motion)
    {
        return ECombatInputResult::Rejected;
    }

    if (Phase == ECombatInputPhase::Canceled)
    {
        if (Command == ECombatCommand::BasicAttack)
        {
            ResetBasicPress();
        }
        if (Command == BufferedCommand)
        {
            ClearBufferedInput();
        }
        return ECombatInputResult::Ignored;
    }
    const bool bLocked = IsActionLocked();
    const bool bBasicTap = Command == ECombatCommand::BasicAttack
        && (Phase == ECombatInputPhase::Pressed || Phase == ECombatInputPhase::Released);
    const bool bBasicFollowUp = bBasicTap && HasBasicFollowUp();
    if (bLocked && !bBasicFollowUp)
    {
        return ECombatInputResult::Rejected;
    }
    if (Command == ECombatCommand::BasicAttack)
    {
        if (Phase == ECombatInputPhase::Released && BasicPressedAt >= 0.0)
        {
            return ReleaseBasicPress();
        }
        // 重击只接受收势窗里的新短按，持续按住不能把它再次接成重击。
        if (bLocked && Phase == ECombatInputPhase::Pressed && Motion->GetBasicFollowUpWindow() == ECombatChainWindow::Closed)
        {
            return ECombatInputResult::Rejected;
        }
        if (Phase == ECombatInputPhase::Pressed && (bLocked || !Scripts->ResolveAction(this, Command, ECombatInputPhase::Held).IsNone()))
        {
            return BeginBasicPress(Scripts->ResolveAction(this, Command, Phase));
        }
    }
    const FName ActionId = Scripts->ResolveAction(this, Command, Phase);
    if (ActionId.IsNone())
    {
        return ECombatInputResult::Ignored;
    }

    if (ActiveInstanceId != 0)
    {
        if (Command == ECombatCommand::BasicAttack && Phase == ECombatInputPhase::Held)
        {
            if (ActiveCommand == Command && Motion->IsInputBufferOpen())
            {
                return StartAction(Command, ActionId, ECombatActionEvent::Chained);
            }
            // 普攻窗口未到时等待开窗；已过窗或处于其他技能时，等待该实例完整结束。
            return BufferInput(Command, Phase, ActionId, ActiveCommand == Command && Motion->IsBeforeChainWindow());
        }
        // 黄球在松手或长按成立时才选出动作，两者都按一次发动意图处理。
        const bool bYellowActivation = Command == ECombatCommand::YellowOrb
            && (Phase == ECombatInputPhase::Held || Phase == ECombatInputPhase::Released);
        const bool bActivation = Phase == ECombatInputPhase::Pressed || bYellowActivation;
        if (bActivation && ActiveCommand == ECombatCommand::BasicAttack && Command != ActiveCommand)
        {
            return TryInterruptBasicAttack(Command, ActionId);
        }
        const bool bCanChain = bBasicFollowUp || Command == ActiveCommand || (IsSignalOrb(Command) && IsSignalOrb(ActiveCommand));
        const ECombatChainWindow Window = bBasicFollowUp ? Motion->GetBasicFollowUpWindow() : Motion->GetChainWindow();
        if (!bActivation || !bCanChain || Window == ECombatChainWindow::Closed)
        {
            NotifyAction(ActionId, 0, ECombatActionEvent::Rejected);
            return ECombatInputResult::Rejected;
        }
        if (Window == ECombatChainWindow::Buffer)
        {
            return BufferInput(Command, Phase, ActionId, true);
        }
    }
    return StartAction(Command, ActionId, ECombatActionEvent::Chained);
}

// 盾斧待机按下立即起首段，其他状态保留按下时的资格以区分轻击与重击。
ECombatInputResult UCombatComponent::BeginBasicPress(FName TapAction)
{
    if (BasicPressedAt >= 0.0 || (!BufferedActionId.IsNone() && BufferedPhase == ECombatInputPhase::Held))
    {
        return ECombatInputResult::Buffered;
    }

    const bool bIdleLight = ActiveInstanceId == 0 && !TapAction.IsNone();
    if (bIdleLight)
    {
        const auto Result = StartAction(ECombatCommand::BasicAttack, TapAction, ECombatActionEvent::Chained);
        if (Result != ECombatInputResult::Started || ActiveActionId != TapAction)
        {
            return Result;
        }
    }
    // 首段已经发动，松手只结束手势；若持续按住，达到阈值后按首段的衔接窗转重击。
    TapActionAtPress = bIdleLight ? NAME_None : TapAction;
    const ECombatChainWindow Window = HasBasicFollowUp() ? Motion->GetBasicFollowUpWindow() : Motion->GetChainWindow();
    TapWindowInstanceId = !bIdleLight && Window != ECombatChainWindow::Closed ? ActiveInstanceId : 0;
    BasicPressedAt = GetWorld()->GetTimeSeconds();
    GetWorld()->GetTimerManager().SetTimer(BasicHoldTimer, this, &ThisClass::ConfirmBasicHold, BasicHoldThresholdSeconds, false);
    return bIdleLight ? ECombatInputResult::Started : ECombatInputResult::Buffered;
}

// 在当初按下的窗口内确认轻击；若计时器本帧尚未更新，仍按实际持续时间判为长按。
ECombatInputResult UCombatComponent::ReleaseBasicPress()
{
    const bool bHeld = GetWorld()->GetTimeSeconds() - BasicPressedAt >= BasicHoldThresholdSeconds;
    const FName TapAction = TapActionAtPress;
    const bool bAccepted = TapWindowInstanceId != 0 && TapWindowInstanceId == ActiveInstanceId;
    ResetBasicPress();
    if (bHeld)
    {
        return SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Held);
    }
    if (TapAction.IsNone())
    {
        return ECombatInputResult::Ignored;
    }
    if (!bAccepted)
    {
        NotifyAction(TapAction, 0, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }
    if (HasBasicFollowUp() ? !Motion->HasReachedBasicFollowUpWindow() : Motion->IsBeforeChainWindow())
    {
        return BufferInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed, TapAction, true);
    }
    return StartAction(ECombatCommand::BasicAttack, TapAction, ECombatActionEvent::Chained);
}

// 达到阈值就提交同一次按键的长按意图，后续松手不会补发轻击。
void UCombatComponent::ConfirmBasicHold()
{
    ResetBasicPress();
    SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Held);
}

// 丢弃尚未确认的手势，防止失焦或打断后的计时器触发旧输入。
void UCombatComponent::ResetBasicPress()
{
    BasicPressedAt = -1.0;
    TapActionAtPress = NAME_None;
    TapWindowInstanceId = 0;
    GetWorld()->GetTimerManager().ClearTimer(BasicHoldTimer);
}

// 只需要等衔接窗口时才轮询；必须等结束的输入由 Montage 自然结束回调消费。
ECombatInputResult UCombatComponent::BufferInput(ECombatCommand Command, ECombatInputPhase Phase, FName ActionId, bool bWaitForWindow)
{
    BufferedCommand = Command;
    BufferedPhase = Phase;
    BufferedActionId = ActionId;
    SetComponentTickEnabled(bWaitForWindow);
    return ECombatInputResult::Buffered;
}

// 读取活动实例的完整播放标记，已结束实例不会继续阻止输入。
bool UCombatComponent::IsActionLocked() const
{
    return ActiveInstanceId != 0 && Motion && Motion->RequiresFullPlayback();
}

// 动作配置决定能否跨指令接普攻，空闲或普通动作不继承上一招的窗口。
bool UCombatComponent::HasBasicFollowUp() const
{
    return ActiveInstanceId != 0 && Motion && Motion->HasBasicFollowUp();
}

// 检查替换指令能否打断普攻，符合条件时由新动作接管实例。
ECombatInputResult UCombatComponent::TryInterruptBasicAttack(ECombatCommand Command, FName ActionId)
{
    if (ActiveCommand != ECombatCommand::BasicAttack || !IsSignalOrb(Command))
    {
        NotifyAction(ActionId, 0, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }
    return StartAction(Command, ActionId, ECombatActionEvent::Interrupted);
}

// 先尝试起播，再更新当前实例并通知被替换的动作。
ECombatInputResult UCombatComponent::StartAction(ECombatCommand Command, FName ActionId, ECombatActionEvent PreviousEvent)
{
    const int64 PreviousInstanceId = ActiveInstanceId;
    const FName PreviousActionId = ActiveActionId;
    const bool bPreserveBasicHold = PreviousInstanceId != 0 && PreviousEvent == ECombatActionEvent::Chained
        && Command == ECombatCommand::BasicAttack && ActiveCommand == Command && !IsActionLocked();
    const int64 InstanceId = ++LastInstanceId;
    const FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &UCombatComponent::OnMontageEnded, InstanceId);
    if (!Motion->PlayAction(ActionId, Command, Ended))
    {
        NotifyAction(ActionId, InstanceId, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }

    if (bPreserveBasicHold)
    {
        // 前一个按键的缓存刚续段，新按键仍可变成长按，但不能重复确认旧段轻击。
        TapActionAtPress = NAME_None;
    }
    else
    {
        ResetBasicPress();
    }
    ClearBufferedInput();
    ActiveInstanceId = InstanceId;
    ActiveActionId = ActionId;
    ActiveCommand = Command;
    if (PreviousInstanceId != 0)
    {
        NotifyAction(PreviousActionId, PreviousInstanceId, PreviousEvent);
    }

    // 旧动作的监听者可能打断新实例，起手通知仅发给当前实例。
    if (ActiveInstanceId == InstanceId)
    {
        NotifyAction(ActionId, InstanceId, ECombatActionEvent::Started);
    }
    return ECombatInputResult::Started;
}

// 缓存窗口与衔接窗口分别计时，只有动画实际前进到衔接窗口才会起播下一段。
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    const bool bBasicFollowUp = BufferedCommand == ECombatCommand::BasicAttack
        && BufferedPhase == ECombatInputPhase::Pressed && HasBasicFollowUp();
    if (Motion && (bBasicFollowUp ? Motion->HasReachedBasicFollowUpWindow() : Motion->HasReachedChainWindow()))
    {
        ConsumeBufferedInput(ECombatActionEvent::Chained);
    }
}

// 重新检查此刻形态和段号，只消费本实例的一次输入。
bool UCombatComponent::ConsumeBufferedInput(ECombatActionEvent PreviousEvent)
{
    if (BufferedActionId.IsNone() || bEndingPlay)
    {
        return false;
    }
    const FName ActionId = BufferedActionId;
    const ECombatCommand Command = BufferedCommand;
    const ECombatInputPhase Phase = BufferedPhase;
    ClearBufferedInput();
    UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UCombatScriptSubsystem* Scripts = Game ? Game->GetSubsystem<UCombatScriptSubsystem>() : nullptr;
    if (!Scripts || Scripts->ResolveAction(this, Command, Phase) != ActionId)
    {
        return false;
    }
    return StartAction(Command, ActionId, PreviousEvent) == ECombatInputResult::Started;
}

// 起播新招、取消输入或结束实例时撤销旧招的缓冲。
void UCombatComponent::ClearBufferedInput()
{
    BufferedActionId = NAME_None;
    SetComponentTickEnabled(false);
}

// 普通打断遵守完整播放保护，组件退出时仍可停止当前动作。
void UCombatComponent::InterruptActiveAction()
{
    if (ActiveInstanceId != 0 && (bEndingPlay || !IsActionLocked()))
    {
        StopActiveAction();
    }
}

// 已缓存的出招和在当前窗口受理的轻击优先，方向键只在允许的时段接管。
bool UCombatComponent::TryCancelForMovement()
{
    const bool bPendingTap = TapWindowInstanceId == ActiveInstanceId && !TapActionAtPress.IsNone();
    if (bEndingPlay || ActiveInstanceId == 0 || !Motion || !BufferedActionId.IsNone()
        || bPendingTap || !Motion->IsMoveCancelWindowOpen())
    {
        return false;
    }
    StopActiveAction();
    return ActiveInstanceId == 0;
}

// 保留被打断的实例号，延迟到达的动画结束回调会由结算逻辑去重。
void UCombatComponent::StopActiveAction()
{
    const int64 InstanceId = ActiveInstanceId;
    Motion->StopAction();
    FinishAction(InstanceId, ECombatActionEvent::Interrupted);
}

// 将蒙太奇的结束回调转成对应实例的自然收招或打断事件。
void UCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId)
{
    // 极低帧率下动画可能一次推进到结束；只补消费本实例自然经过的衔接窗。
    if (!bInterrupted && InstanceId == ActiveInstanceId && ConsumeBufferedInput(ECombatActionEvent::Completed))
    {
        return;
    }
    FinishAction(InstanceId, bInterrupted ? ECombatActionEvent::Interrupted : ECombatActionEvent::Completed);
}

// 只结算当前动作的结束事件，旧段淡出回调不能影响正在播放的新段。
void UCombatComponent::FinishAction(int64 InstanceId, ECombatActionEvent Event)
{
    // 旧 Montage 的结束通知可能晚于下一次起手，只允许它回收自己的实例。
    if (InstanceId != ActiveInstanceId)
    {
        return;
    }

    const FName ActionId = ActiveActionId;
    // 锁定动作中的按住只可被确认为本实例的短按，不能在自然收刀后变成下一次重击。
    if (Event == ECombatActionEvent::Interrupted || IsActionLocked())
    {
        ResetBasicPress();
    }
    ClearBufferedInput();
    ActiveActionId = NAME_None;
    ActiveInstanceId = 0;
    NotifyAction(ActionId, InstanceId, Event);
}

// 将动作编号、实例编号和生命周期事件写入日志并广播。
void UCombatComponent::NotifyAction(FName ActionId, int64 InstanceId, ECombatActionEvent Event)
{
    UE_LOG(LogPGRCombat, Display, TEXT("Combat action %s Action=%s Instance=%lld"),
           *UEnum::GetValueAsString(Event), *ActionId.ToString(), InstanceId);
    OnActionChanged.Broadcast(ActionId, InstanceId, Event);
}
