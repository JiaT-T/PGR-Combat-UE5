#include "Combat/NanamiOrbComponent.h"

#include "Combat/CombatComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Scripting/CombatScriptSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNanamiOrbs, Log, All);

// 每个角色从自己的训练初值出发，让 HUD 在初次读取或收到通知时看到相同的状态。
void UNanamiOrbComponent::BeginPlay()
{
    Super::BeginPlay();
    OrbState = FNanamiOrbState();
    OrbState.Stars = FMath::Clamp(InitialStars, 0, 2);
    LastAppliedInstanceId = 0;
    Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
    if (Combat)
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
    Super::EndPlay(EndPlayReason);
}

// 只把仍在播放的新实例交给角色脚本，避免旧段结束或重复通知改写球态。
void UNanamiOrbComponent::OnCombatActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event)
{
    if (Event == ECombatActionEvent::Started && Combat && Combat->GetActiveInstanceId() == InstanceId
        && InstanceId > LastAppliedInstanceId)
    {
        const UGameInstance* Game = GetWorld()->GetGameInstance();
        const auto* Scripts = Game ? Game->GetSubsystem<UCombatScriptSubsystem>() : nullptr;
        if (Scripts)
        {
            Scripts->NanamiOrbActionStarted.ExecuteIfBound(this, ActionId, InstanceId);
        }
    }
}

// 校验实例和支出后一起发布新状态；三消结算数与实际扣除的库存分别记录。
void UNanamiOrbComponent::ApplyRedOrbResult(int64 InstanceId, int32 SpentOrbs, int32 EffectivePings, int32 SpentStars,
                                         bool bFollowUp, bool bTriplePing)
{
    if (!Combat || Combat->GetActiveInstanceId() != InstanceId || InstanceId <= LastAppliedInstanceId
        || !FMath::IsWithinInclusive(SpentOrbs, 0, OrbState.RedStock) || !FMath::IsWithinInclusive(EffectivePings, 0, 3)
        || !FMath::IsWithinInclusive(SpentStars, 0, OrbState.Stars))
    {
        return;
    }

    LastAppliedInstanceId = InstanceId;
    OrbState.RedStock -= SpentOrbs;
    OrbState.Stars -= SpentStars;
    OrbState.bRedFollowUp = bFollowUp;
    OrbState.bRedTriplePing = bTriplePing;

    UE_LOG(LogNanamiOrbs, Display, TEXT("Red orb instance=%lld spent=%d pings=%d red=%d blue=%d stars=%d followup=%d triple=%d"),
           InstanceId, SpentOrbs, EffectivePings, OrbState.RedStock, OrbState.BlueStock, OrbState.Stars, bFollowUp, bTriplePing);
    OnOrbStateChanged.Broadcast(OrbState);
}
