#pragma once

#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class UAnimMontage;
class UCombatMotionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCombatActionChanged, FName, ActionId, int64, InstanceId, ECombatActionEvent, Event);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 提交带有按键阶段的指令，并返回本次受理结果。
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ECombatInputResult SubmitInput(ECombatCommand Command, ECombatInputPhase Phase);

    // 主动打断当前动作并丢弃其连段缓存。
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void InterruptActiveAction();

    // 返回正在播放的动作标识，空闲时返回空标识。
    UFUNCTION(BlueprintPure, Category = "Combat")
    FName GetActiveActionId() const { return ActiveActionId; }

    // 返回本次出招的实例编号，空闲时返回零。
    UFUNCTION(BlueprintPure, Category = "Combat")
    int64 GetActiveInstanceId() const { return ActiveInstanceId; }

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FCombatActionChanged OnActionChanged;

protected:
    // 进入世界时连接角色的动作组件。
    virtual void BeginPlay() override;
    // 退出世界时停止受理输入并结束动作。
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    // 接收指定动作实例的动画结束通知。
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId);
    // 结束给定实例并处理其连段缓存。
    void FinishAction(int64 InstanceId, ECombatActionEvent Event);
    // 分发一次动作生命周期通知。
    void NotifyAction(FName ActionId, int64 InstanceId, ECombatActionEvent Event);

    UPROPERTY(Transient)
    TObjectPtr<UCombatMotionComponent> Motion = nullptr;

    FName ActiveActionId = NAME_None;
    int64 ActiveInstanceId = 0;
    int64 LastInstanceId = 0;
    bool bBasicAttackBuffered = false;
    bool bEndingPlay = false;
};
