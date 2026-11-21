#include "Combat/CombatComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Motion/CombatMotionComponent.h"
#include "Scripting/CombatScriptSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRCombat, Log, All);

namespace
{
    // 技能球共享衔接窗口，并可在确认发动时打断当前普攻。
    bool IsSignalOrb(ECombatCommand Command)
    {
        return Command == ECombatCommand::RedOrb || Command == ECombatCommand::BlueOrb || Command == ECombatCommand::YellowOrb;
    }
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

    const FName ActionId = Scripts->ResolveAction(this, Command, Phase);
    if (ActionId.IsNone())
    {
        return ECombatInputResult::Ignored;
    }

    if (ActiveInstanceId != 0)
    {
        // 黄球在松手或长按成立时才选出动作，两者都按一次发动意图处理。
        const bool bYellowActivation = Command == ECombatCommand::YellowOrb
            && (Phase == ECombatInputPhase::Held || Phase == ECombatInputPhase::Released);
        const bool bActivation = Phase == ECombatInputPhase::Pressed || bYellowActivation;
        if (bActivation && ActiveCommand == ECombatCommand::BasicAttack && Command != ActiveCommand)
        {
            return TryInterruptBasicAttack(Command, ActionId);
        }
        const bool bCanChain = Command == ActiveCommand || (IsSignalOrb(Command) && IsSignalOrb(ActiveCommand));
        if (!bActivation || !bCanChain || !Motion->IsInputBufferOpen())
        {
            NotifyAction(ActionId, 0, ECombatActionEvent::Rejected);
            return ECombatInputResult::Rejected;
        }
    }
    return StartAction(Command, ActionId, ECombatActionEvent::Chained);
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
    const int64 InstanceId = ++LastInstanceId;
    const FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &UCombatComponent::OnMontageEnded, InstanceId);
    if (!Motion->PlayAction(ActionId, Command, Ended))
    {
        NotifyAction(ActionId, InstanceId, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }

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

// 停止当前动画，并用其原实例编号结算打断。
void UCombatComponent::InterruptActiveAction()
{
    const int64 InstanceId = ActiveInstanceId;
    if (InstanceId != 0)
    {
        Motion->StopAction();
        FinishAction(InstanceId, ECombatActionEvent::Interrupted);
    }
}

// 将蒙太奇的结束回调转成对应实例的自然收招或打断事件。
void UCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId)
{
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
