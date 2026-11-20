#include "Combat/NanamiOrbComponent.h"

#include "Combat/CombatComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "Scripting/CombatScriptSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNanamiOrbs, Log, All);

// 读取本角色的训练预设，并在战斗组件和脚本宿主可用时注册动作回调。
void UNanamiOrbComponent::BeginPlay()
{
    Super::BeginPlay();
    OrbState = FNanamiOrbState();
    OrbState.RedStock = FMath::Clamp(InitialRedStock, 0, MaxStock);
    OrbState.BlueStock = FMath::Clamp(InitialBlueStock, 0, MaxStock - OrbState.RedStock);
    OrbState.Stars = FMath::Clamp(InitialStars, 0, 2);
    LastAppliedInstanceId = 0;
    Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
    ScriptHost = UGameInstance::GetSubsystem<UCombatScriptSubsystem>(GetWorld()->GetGameInstance());
    if (Combat && ScriptHost)
    {
        Combat->OnActionChanged.AddUniqueDynamic(this, &ThisClass::OnCombatActionChanged);
    }
    OnOrbStateChanged.Broadcast(OrbState);
}

// 先解除战斗回调，以免角色退出后继续接收动作通知。
void UNanamiOrbComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Combat)
    {
        Combat->OnActionChanged.RemoveDynamic(this, &ThisClass::OnCombatActionChanged);
        Combat = nullptr;
    }
    GetWorld()->GetTimerManager().ClearTimer(RedFollowUpTimer);
    GetWorld()->GetTimerManager().ClearTimer(BlueFollowUpTimer);
    ScriptHost = nullptr;
    Super::EndPlay(EndPlayReason);
}

// 只把仍在播放的新实例交给角色脚本，避免旧段结束或重复通知改写球态。
void UNanamiOrbComponent::OnCombatActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event)
{
    if (Event == ECombatActionEvent::Started && Combat && Combat->GetActiveInstanceId() == InstanceId
        && InstanceId > LastAppliedInstanceId)
    {
        ScriptHost->NanamiOrbActionStarted.ExecuteIfBound(this, ActionId, InstanceId);
    }
}

// 校验实例和支出后一起发布新状态；三消结算数与实际扣除的库存分别记录。
void UNanamiOrbComponent::ApplyOrbResult(int64 InstanceId, bool bBlue, int32 SpentOrbs, int32 EffectivePings, int32 SpentStars,
                                         bool bFollowUp, bool bTriplePing)
{
    int32& Stock = bBlue ? OrbState.BlueStock : OrbState.RedStock;
    if (!Combat || Combat->GetActiveInstanceId() != InstanceId || InstanceId <= LastAppliedInstanceId
        || !FMath::IsWithinInclusive(SpentOrbs, 0, Stock) || !FMath::IsWithinInclusive(EffectivePings, 0, 3)
        || !FMath::IsWithinInclusive(SpentStars, 0, OrbState.Stars))
    {
        return;
    }

    LastAppliedInstanceId = InstanceId;
    Stock -= SpentOrbs;
    OrbState.Stars -= SpentStars;
    (bBlue ? OrbState.bBlueTriplePing : OrbState.bRedTriplePing) = bTriplePing;
    SetFollowUp(bBlue, bFollowUp);
    // 使用亮星结算二段时，两色同时获得续段，或在最后一星耗尽时一起回退。
    if (SpentStars > 0)
    {
        SetFollowUp(!bBlue, bFollowUp);
    }

    UE_LOG(LogNanamiOrbs, Display, TEXT("%s orb instance=%lld spent=%d pings=%d red=%d blue=%d stars=%d redFollow=%d blueFollow=%d"),
           bBlue ? TEXT("Blue") : TEXT("Red"), InstanceId, SpentOrbs, EffectivePings,
           OrbState.RedStock, OrbState.BlueStock, OrbState.Stars, OrbState.bRedFollowUp, OrbState.bBlueFollowUp);
    OnOrbStateChanged.Broadcast(OrbState);
}

// 每色独立计时；刷新一种颜色不会推迟另一种颜色自然到期。
void UNanamiOrbComponent::SetFollowUp(bool bBlue, bool bEnabled)
{
    (bBlue ? OrbState.bBlueFollowUp : OrbState.bRedFollowUp) = bEnabled;
    FTimerHandle& Timer = bBlue ? BlueFollowUpTimer : RedFollowUpTimer;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (bEnabled)
    {
        Timers.SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::ExpireFollowUp, bBlue), FollowUpDuration, false);
    }
    else
    {
        Timers.ClearTimer(Timer);
    }
}

// 二段计时结束后更新 HUD，库存和已有星数保留。
void UNanamiOrbComponent::ExpireFollowUp(bool bBlue)
{
    (bBlue ? OrbState.bBlueFollowUp : OrbState.bRedFollowUp) = false;
    OnOrbStateChanged.Broadcast(OrbState);
}
