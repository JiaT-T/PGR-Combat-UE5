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
    // 默认从起点播放整段 Montage；选择 Section 时只播放指定段。
    UPROPERTY(EditAnywhere, Category = "Task")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, Category = "Task")
    FName Section = NAME_None;

    // 可主动提前结束攻击的 Montage 在这里指定收势段。
    UPROPERTY(EditAnywhere, Category = "Task")
    FName ExitSection = NAME_None;

    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0.01"))
    float PlayRate = 1.f;
};

UCLASS(Abstract)
class PGRCOMBAT_API UCombatWindowTask : public UCombatAbilityTask
{
    GENERATED_BODY()

public:
    // 默认从 Montage 起点计时；指定参考 Section 的 Task 从该段起点计时。
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

// 为本招单独指定允许交还移动控制的时间。
UCLASS(meta = (DisplayName = "Movement Cancel Window"))
class PGRCOMBAT_API UCombatMoveCancelWindowTask : public UCombatWindowTask
{
    GENERATED_BODY()
};

// 此动作仅在指定区间允许必杀接管。指定 Section 时起止秒数从该段入口计。
UCLASS(meta = (DisplayName = "Ultimate Interrupt Window"))
class PGRCOMBAT_API UCombatUltimateWindowTask : public UCombatWindowTask
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Task")
    FName RelativeToSection = NAME_None;

    // 开窗前可记住一次必杀输入，提前量不会跨入上一段。
    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float BufferLeadSeconds = 0.f;
};

// 允许从本招的收势接普攻。三个时间点均从完整 Montage 起点算起。
UCLASS(meta = (DisplayName = "Basic Attack Follow-up"))
class PGRCOMBAT_API UCombatBasicFollowUpTask : public UCombatAbilityTask
{
    GENERATED_BODY()

public:
    // 从此处开始接收提前按下的普攻。
    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float BufferStartSeconds = 0.f;

    // 从此处起消费缓存，或在按下普攻时立即混入下一招。
    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float ChainStartSeconds = 0.f;

    UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0", Units = "s"))
    float EndSeconds = 0.f;
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
    FName Section = NAME_None;
    FName ExitSection = NAME_None;
    float StartPosition = 0.f;
    float PlayRate = 1.f;
    bool bRequireFullPlayback = false;
    FCombatAbilityWindow InputBuffer;
    FCombatAbilityWindow ChainWindow;
    FCombatAbilityWindow MoveCancelWindow;
    FCombatAbilityWindow BasicFollowUpBuffer;
    FCombatAbilityWindow BasicFollowUpChain;
    FCombatAbilityWindow UltimateBuffer;
    FCombatAbilityWindow UltimateChain;
};

// 每个子蓝图配置一招。类默认值只保存编排，每次播放和缓存的状态属于角色。
UCLASS(Abstract, Blueprintable)
class PGRCOMBAT_API UCombatMontageAbility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Ability")
    ECombatCommand Command = ECombatCommand::BasicAttack;

    // 完整播放保护普通输入和移动，专用窗口分别控制普攻、移动取消与必杀接管。
    UPROPERTY(EditDefaultsOnly, Category = "Ability")
    bool bRequireFullPlayback = false;

    // 至少需要一个播放 Task，提前输入和立即衔接分别按需添加。
    UPROPERTY(EditDefaultsOnly, Instanced, Category = "Ability")
    TArray<TObjectPtr<UCombatAbilityTask>> Tasks;

    // 在启动前验证 Task 组合并解析为只供本次播放使用的快照。
    bool ResolveTasks(FCombatMontageTasks& OutTasks) const;
};
