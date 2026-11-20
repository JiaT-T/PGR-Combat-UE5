#pragma once

#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "CombatMotionComponent.generated.h"

class UCombatActionSet;
class UAnimInstance;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UCombatMotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Combat")
    TObjectPtr<UCombatActionSet> ActionSet = nullptr;

    // 尝试按动作标识播放动画，并在该实例结束时调用回调。
    bool PlayAction(FName ActionId, FOnMontageEnded OnEnded);
    // 查询当前动作是否处于可立即衔接下一段的时段。
    bool IsInputBufferOpen() const;
    // 立即停止本组件跟踪的动作动画。
    void StopAction();

private:
    TWeakObjectPtr<UAnimInstance> PlayingAnimInstance;
    int32 MontageInstanceId = INDEX_NONE;
    float BufferStart = 0.0f;
    float BufferEnd = 0.0f;
};
