#pragma once

#include "Combat/CombatTypes.h"
#include "UObject/Object.h"
#include "CombatMontageAbility.generated.h"

class UAnimMontage;

// Task 的配置只存在于 Ability 蓝图上，不随每一段播放修改。
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PGRCOMBAT_API UCombatAbilityTask : public UObject
{
    GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "Play Montage"))
class PGRCOMBAT_API UCombatPlayMontageTask : public UCombatAbilityTask
{
    GENERATED_BODY()

public:
    // 从头以原速播放完整动作。
    UPROPERTY(EditAnywhere, Category = "Task")
    TObjectPtr<UAnimMontage> Montage = nullptr;
};

UCLASS(Abstract)
class PGRCOMBAT_API UCombatWindowTask : public UCombatAbilityTask
{
    GENERATED_BODY()

public:
    // 从完整 Montage 起点计时，单位为秒。
    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float StartSeconds = 0.f;

    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float EndSeconds = 0.f;
};

UCLASS(meta = (DisplayName = "Input Buffer"))
class PGRCOMBAT_API UCombatInputBufferTask : public UCombatWindowTask
{
    GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "Chain Window"))
class PGRCOMBAT_API UCombatChainWindowTask : public UCombatWindowTask
{
    GENERATED_BODY()
};

// 解析后的窗口快照属于播放实例，默认空区间不会接收输入。
struct FCombatAbilityWindow
{
    float StartSeconds = 0.f;
    float EndSeconds = 0.f;

    // 检查窗口是否已启用并包含给定的本地播放位置。
    bool Contains(float Position) const
    {
        return StartSeconds < EndSeconds && Position >= StartSeconds && Position <= EndSeconds;
    }
};

struct FCombatMontageTasks
{
    UAnimMontage* Montage = nullptr;
    FCombatAbilityWindow InputBuffer;
    FCombatAbilityWindow ChainWindow;
};

// 每个子蓝图配置一招。类默认值只保存编排，每次播放和缓存的状态属于角色。
UCLASS(Abstract, Blueprintable)
class PGRCOMBAT_API UCombatMontageAbility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Ability")
    ECombatCommand Command = ECombatCommand::BasicAttack;

    // 至少需要一个播放 Task，提前输入和立即衔接分别按需添加。
    UPROPERTY(EditDefaultsOnly, Instanced, Category = "Ability")
    TArray<TObjectPtr<UCombatAbilityTask>> Tasks;

    // 在启动前验证 Task 组合并解析为只供本次播放使用的快照。
    bool ResolveTasks(FCombatMontageTasks& OutTasks) const;
};
