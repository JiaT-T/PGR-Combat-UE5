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

// 按信号球颜色读取训练场的同色组大小。
int32 UNanamiOrbComponent::GetTrainingGroupSize(ENanamiOrbColor Color) const
{
    switch (Color)
    {
    case ENanamiOrbColor::Red:
        return TrainingRedGroupSize;
    case ENanamiOrbColor::Blue:
        return TrainingBlueGroupSize;
    default:
        return 0;
    }
}

// 校验实例和支出后一起发布新状态；三消结算数与实际扣除的库存分别记录。
void UNanamiOrbComponent::ApplyOrbResult(int64 InstanceId, ENanamiOrbColor Color, int32 SpentOrbs, int32 EffectivePings,
                                         int32 SpentStars, bool bFollowUp, bool bTriplePing)
{
    if (!Combat || Combat->GetActiveInstanceId() != InstanceId || InstanceId <= LastAppliedInstanceId
        || SpentOrbs < 0 || !FMath::IsWithinInclusive(EffectivePings, 0, 3)
        || !FMath::IsWithinInclusive(SpentStars, 0, OrbState.Stars))
    {
        return;
    }

    switch (Color)
    {
    case ENanamiOrbColor::Red:
        if (SpentOrbs > OrbState.RedStock)
        {
            return;
        }
        OrbState.RedStock -= SpentOrbs;
        OrbState.bRedTriplePing = bTriplePing;
        break;
    case ENanamiOrbColor::Blue:
        if (SpentOrbs > OrbState.BlueStock)
        {
            return;
        }
        OrbState.BlueStock -= SpentOrbs;
        OrbState.bBlueTriplePing = bTriplePing;
        break;
    default:
        return;
    }

    LastAppliedInstanceId = InstanceId;
    OrbState.Stars -= SpentStars;
    // 使用亮星结算二段时，两色同时获得续段，或在最后一星耗尽时一起回退。
    if (SpentStars > 0)
    {
        SetFollowUp(ENanamiOrbColor::Red, bFollowUp);
        SetFollowUp(ENanamiOrbColor::Blue, bFollowUp);
    }
    else
    {
        SetFollowUp(Color, bFollowUp);
    }

    UE_LOG(LogNanamiOrbs, Display, TEXT("%s orb instance=%lld spent=%d pings=%d red=%d blue=%d stars=%d redFollow=%d blueFollow=%d"),
           *StaticEnum<ENanamiOrbColor>()->GetNameStringByValue(static_cast<int64>(Color)), InstanceId, SpentOrbs, EffectivePings,
           OrbState.RedStock, OrbState.BlueStock, OrbState.Stars, OrbState.bRedFollowUp, OrbState.bBlueFollowUp);
    OnOrbStateChanged.Broadcast(OrbState);
}

// 每色独立计时；刷新一种颜色不会推迟另一种颜色自然到期。
void UNanamiOrbComponent::SetFollowUp(ENanamiOrbColor Color, bool bEnabled)
{
    FTimerHandle* Timer;
    switch (Color)
    {
    case ENanamiOrbColor::Red:
        OrbState.bRedFollowUp = bEnabled;
        Timer = &RedFollowUpTimer;
        break;
    case ENanamiOrbColor::Blue:
        OrbState.bBlueFollowUp = bEnabled;
        Timer = &BlueFollowUpTimer;
        break;
    default:
        return;
    }

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (bEnabled)
    {
        Timers.SetTimer(*Timer, FTimerDelegate::CreateUObject(this, &ThisClass::ExpireFollowUp, Color), FollowUpDuration, false);
    }
    else
    {
        Timers.ClearTimer(*Timer);
    }
}

// 到期时按回调携带的颜色收回该色二段，并通知 HUD。
void UNanamiOrbComponent::ExpireFollowUp(ENanamiOrbColor Color)
{
    SetFollowUp(Color, false);
    OnOrbStateChanged.Broadcast(OrbState);
}
