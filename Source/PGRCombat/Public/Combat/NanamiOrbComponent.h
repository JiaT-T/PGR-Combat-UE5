#pragma once

#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "NanamiOrbComponent.generated.h"

class UCombatComponent;
class UCombatScriptSubsystem;

USTRUCT(BlueprintType)
struct PGRCOMBAT_API FNanamiOrbState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    int32 RedStock = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    int32 BlueStock = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    int32 Stars = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    bool bRedFollowUp = false;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    bool bBlueFollowUp = false;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    bool bRedTriplePing = false;

    UPROPERTY(BlueprintReadOnly, Category = "Orbs")
    bool bBlueTriplePing = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNanamiOrbStateChanged, const FNanamiOrbState&, State);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UNanamiOrbComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxStock = 16;

    // 返回此角色的信号球快照，供脚本选招及界面首次显示使用。
    UFUNCTION(BlueprintPure, Category = "Orbs")
    FNanamiOrbState GetOrbState() const { return OrbState; }

    // 返回训练场为普通红二段预置的同色组大小，实际原作选组策略待验证。
    UFUNCTION(BlueprintPure, Category = "Orbs")
    int32 GetTrainingRedGroupSize() const { return TrainingRedGroupSize; }

    // 将脚本计算出的红球结算一次应用到仍有效的动作实例。
    UFUNCTION()
    void ApplyRedOrbResult(int64 InstanceId, int32 SpentOrbs, int32 EffectivePings, int32 SpentStars,
                           bool bFollowUp, bool bTriplePing);

    UPROPERTY(BlueprintAssignable, Category = "Orbs")
    FNanamiOrbStateChanged OnOrbStateChanged;

protected:
    // 应用训练预设并监听同一角色已确认起播的动作。
    virtual void BeginPlay() override;
    // 退出关卡时移除本实例的订阅。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 仅在动作成功开始后通知脚本结算，淡出和结束回调不扣资源。
    UFUNCTION()
    void OnCombatActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event);

    // 当前训练用例的起始库存。原作的入场库存规则确认后再提供相应预设。
    UPROPERTY(EditAnywhere, Category = "Training|Orbs", meta = (ClampMin = "0", ClampMax = "16"))
    int32 InitialRedStock = 5;

    UPROPERTY(EditAnywhere, Category = "Training|Orbs", meta = (ClampMin = "0", ClampMax = "16"))
    int32 InitialBlueStock = 3;

    UPROPERTY(EditAnywhere, Category = "Training|Orbs", meta = (ClampMin = "0", ClampMax = "2"))
    int32 InitialStars = 0;

    UPROPERTY(EditAnywhere, Category = "Training|Orbs", meta = (ClampMin = "1", ClampMax = "3"))
    int32 TrainingRedGroupSize = 3;

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> Combat;

    UPROPERTY(Transient)
    TObjectPtr<UCombatScriptSubsystem> ScriptHost;

    FNanamiOrbState OrbState;
    int64 LastAppliedInstanceId = 0;
};
