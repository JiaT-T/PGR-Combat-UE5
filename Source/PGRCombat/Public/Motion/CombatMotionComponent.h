#pragma once

#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatMontageAbility.h"
#include "CombatMotionComponent.generated.h"

class UCombatActionSet;
class UAnimInstance;

enum class ECombatChainWindow : uint8
{
    Closed,
    Buffer,
    Chain
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UCombatMotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Combat")
    TObjectPtr<UCombatActionSet> ActionSet = nullptr;

    // 角色形态可替换指定动作的表现。指令、衔接窗口和实例回调仍来自原动作。
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UAnimMontage>> MontageOverrides;

    // 尝试按动作标识播放动画，并在该实例结束时调用回调。
    bool PlayAction(FName ActionId, ECombatCommand Command, FOnMontageEnded OnEnded);
    // 查询本次按下应当被忽略、暂存，还是立即切到下一段。
    ECombatChainWindow GetChainWindow() const;
    // 兼容已有的立即续段查询，提前输入区间不视作已经开窗。
    bool IsInputBufferOpen() const { return GetChainWindow() == ECombatChainWindow::Chain; }
    // 判断一次已缓存的输入是否随动画越过了允许切招的起点。
    bool HasReachedChainWindow() const;
    // 查询当前实例的衔接窗口是否仍在前方，可等待长按意图。
    bool IsBeforeChainWindow() const;
    // 返回本次实际播放的 Ability 是否要求完整收招。
    bool RequiresFullPlayback() const { return bRequireFullPlayback; }
    // 返回当前实例正在播放的段名，供分段表现读取。
    FName GetCurrentActionSection() const;
    // 检查活动实例是否仍处于指定段之前。
    bool IsBeforeActionSection(FName Section) const;
    // 仅向前跳到指定段，重复请求不会重播已经开始的段。
    bool JumpForwardToActionSection(FName Section);
    // 立即停止本组件跟踪的动作动画。
    void StopAction();

private:
    // 仅访问本组件启动的实例，忽略旧 Montage 的淡出实例。
    FAnimMontageInstance* GetPlayingInstance() const;
    TWeakObjectPtr<UAnimInstance> PlayingAnimInstance;
    int32 MontageInstanceId = INDEX_NONE;
    FCombatAbilityWindow InputBuffer;
    FCombatAbilityWindow ChainWindow;
    bool bRequireFullPlayback = false;
};
