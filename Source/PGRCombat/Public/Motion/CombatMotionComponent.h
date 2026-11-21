#pragma once

#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "CombatMotionComponent.generated.h"

class UCombatActionSet;
class UAnimInstance;
enum class ECombatCommand : uint8;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UCombatMotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Combat")
    TObjectPtr<UCombatActionSet> ActionSet = nullptr;

    // 尝试按动作标识播放动画，并在该实例结束时调用回调。
    bool PlayAction(FName ActionId, ECombatCommand Command, FOnMontageEnded OnEnded);
    // 查询当前动作是否处于可立即衔接下一段的时段。
    bool IsInputBufferOpen() const;
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
    float BufferStart = 0.0f;
    float BufferEnd = 0.0f;
};
