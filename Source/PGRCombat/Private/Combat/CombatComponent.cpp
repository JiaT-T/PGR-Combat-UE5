#include "Combat/CombatComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Motion/CombatMotionComponent.h"
#include "Scripting/CombatScriptSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRCombat, Log, All);

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    bEndingPlay = false;
    Motion = GetOwner()->FindComponentByClass<UCombatMotionComponent>();
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    InterruptActiveAction();
    Super::EndPlay(Reason);
}

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
        if (Command == ECombatCommand::BasicAttack && Phase == ECombatInputPhase::Pressed && Motion->IsInputBufferOpen())
        {
            bBasicAttackBuffered = true;
            return ECombatInputResult::Buffered;
        }
        NotifyAction(ActionId, 0, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }

    const int64 InstanceId = ++LastInstanceId;
    const FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &UCombatComponent::OnMontageEnded, InstanceId);
    if (!Motion->PlayAction(ActionId, Ended))
    {
        NotifyAction(ActionId, InstanceId, ECombatActionEvent::Rejected);
        return ECombatInputResult::Rejected;
    }

    ActiveInstanceId = InstanceId;
    ActiveActionId = ActionId;
    NotifyAction(ActionId, InstanceId, ECombatActionEvent::Started);
    return ECombatInputResult::Started;
}

void UCombatComponent::InterruptActiveAction()
{
    const int64 InstanceId = ActiveInstanceId;
    if (InstanceId != 0)
    {
        Motion->StopAction();
        FinishAction(InstanceId, ECombatActionEvent::Interrupted);
    }
}

void UCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId)
{
    FinishAction(InstanceId, bInterrupted ? ECombatActionEvent::Interrupted : ECombatActionEvent::Completed);
}

void UCombatComponent::FinishAction(int64 InstanceId, ECombatActionEvent Event)
{
    // 旧 Montage 的结束通知可能晚于下一次起手，只允许它回收自己的实例。
    if (InstanceId != ActiveInstanceId)
    {
        return;
    }

    const FName ActionId = ActiveActionId;
    const bool bContinue = bBasicAttackBuffered && Event == ECombatActionEvent::Completed;
    ActiveActionId = NAME_None;
    ActiveInstanceId = 0;
    bBasicAttackBuffered = false;
    NotifyAction(ActionId, InstanceId, Event);

    // 结束事件的订阅者可能已经开始新动作，此时旧连段的缓存已过期。
    if (bContinue && LastInstanceId == InstanceId && !bEndingPlay)
    {
        SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed);
    }
}

void UCombatComponent::NotifyAction(FName ActionId, int64 InstanceId, ECombatActionEvent Event)
{
    UE_LOG(LogPGRCombat, Display, TEXT("Combat action %s Action=%s Instance=%lld"),
           *UEnum::GetValueAsString(Event), *ActionId.ToString(), InstanceId);
    OnActionChanged.Broadcast(ActionId, InstanceId, Event);
}
