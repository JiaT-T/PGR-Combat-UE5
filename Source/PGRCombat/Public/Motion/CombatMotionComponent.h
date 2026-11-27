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

    // 尝试按动作标识播放动画，并在该实例结束时调用回调。
    bool PlayAction(FName ActionId, ECombatCommand Command, FOnMontageEnded OnEnded);
    // 查询本次按下应当被忽略、暂存，还是立即切到下一段。
    ECombatChainWindow GetChainWindow(bool bYellowTap = false) const;
    // 当前实例的播放进度是否已进入移动取消窗口。
    bool IsMoveCancelWindowOpen() const;
    // 读取当前动作交接给移动时的姿势淡出时长。
    float GetMoveBlendOutSeconds() const { return MoveBlendOutSeconds; }
    // 查询当前实例是否配置了专门接普攻的收势窗口，结束回调中仍可读取。
    bool HasBasicFollowUp() const { return BasicFollowUpChain.StartSeconds < BasicFollowUpChain.EndSeconds; }
    // 读取普攻专用窗口，其他指令沿用各自的衔接规则。
    ECombatChainWindow GetBasicFollowUpWindow() const;
    // 判断已缓存的普攻是否到达可混入下一招的起点。
    bool HasReachedBasicFollowUpWindow() const;
    // 有此配置的动作只在自己的必杀窗口允许被替换。
    bool HasUltimateWindow() const { return UltimateChain.StartSeconds < UltimateChain.EndSeconds; }
    // 查询本帧按必杀应拒绝、缓存还是立即接管。
    ECombatChainWindow GetUltimateWindow() const;
    // 判断已接收的必杀输入是否经过开窗点。
    bool HasReachedUltimateWindow() const;
    // 查询当前实例能否接收闪避，含允许的自然淡出区间。
    ECombatChainWindow GetDodgeWindow() const;
    // 检查本实例已记住的闪避是否到达起播点。
    bool HasReachedDodgeWindow() const;
    // 兼容已有的立即续段查询，提前输入区间不视作已经开窗。
    bool IsInputBufferOpen() const { return GetChainWindow() == ECombatChainWindow::Chain; }
    // 判断一次已缓存的输入是否随动画越过了允许切招的起点。
    bool HasReachedChainWindow(bool bYellowTap = false) const;
    // 查询当前实例的衔接窗口是否仍在前方，可等待长按意图。
    bool IsBeforeChainWindow() const;
    // 返回本次实际播放的 Ability 是否要求完整收招。
    bool RequiresFullPlayback() const { return bRequireFullPlayback; }
    // 返回当前实例正在播放的段名，供分段表现读取。
    FName GetCurrentActionSection() const;
    // 取得本次播放配置指定的收势段。
    FName GetExitSection() const { return ExitSection; }
    // 检查活动实例是否仍处于指定段之前。
    bool IsBeforeActionSection(FName Section) const;
    // 仅向前跳到指定段，重复请求不会重播已经开始的段。
    bool JumpForwardToActionSection(FName Section);
    // 解除旧实例的控制权，并按指定时长混出其姿势。
    void StopAction(float BlendOutSeconds = 0.f);

private:
    // 按实例 ID 取当前播放，收势窗口也可以读取该实例自然淡出时的播放位置。
    FAnimMontageInstance* GetPlayingInstance(bool bIncludeNaturalBlendOut = false) const;
    // 只有第一闪处于快速衔接区间时，再次起闪才属于同一组的第二闪。
    bool CanChainSecondDodge() const;
    // 成组闪避的精确帧边界允许微小浮点误差，避免多等一帧。
    float GetDodgeWindowTolerance() const { return DodgePairRestartPosition > 0.f ? UE_KINDA_SMALL_NUMBER : 0.f; }
    // 只有短黄输入且本招配置了专用窗时，才覆盖通用技能球窗口。
    bool UsesYellowTapWindow(bool bYellowTap) const { return bYellowTap && YellowTapChain.StartSeconds < YellowTapChain.EndSeconds; }
    TWeakObjectPtr<UAnimInstance> PlayingAnimInstance;
    int32 MontageInstanceId = INDEX_NONE;
    FName ExitSection = NAME_None;
    FCombatAbilityWindow InputBuffer;
    FCombatAbilityWindow ChainWindow;
    bool bAllowChainDuringNaturalBlendOut = false;
    FCombatAbilityWindow MoveCancelWindow;
    float MoveBlendOutSeconds = 0.f;
    FCombatAbilityWindow BasicFollowUpBuffer;
    FCombatAbilityWindow BasicFollowUpChain;
    FCombatAbilityWindow YellowTapBuffer;
    FCombatAbilityWindow YellowTapChain;
    FCombatAbilityWindow DodgeBuffer;
    FCombatAbilityWindow DodgeChain;
    float DodgePairRestartPosition = 0.f;
    FCombatAbilityWindow UltimateBuffer;
    FCombatAbilityWindow UltimateChain;
    bool bBlendingOutNaturally = false;
    bool bRequireFullPlayback = false;
};
