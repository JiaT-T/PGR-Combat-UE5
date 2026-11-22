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
    // 仅在等待消费提前按下的输入时启用 Tick。
    UCombatComponent();

    // 取消挂在当前实例上的提前输入，角色失去控制时也要清理。
    void ClearBufferedInput();

    // 提交带有按键阶段的指令，并返回本次受理结果。
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ECombatInputResult SubmitInput(ECombatCommand Command, ECombatInputPhase Phase);

    // 主动停止并结算当前动作。
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
    // 根据当前 Montage 的实际进度消费缓存。
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    // 尝试以另一类指令打断当前普攻，仅在新动作成功起播后通知旧实例。
    ECombatInputResult TryInterruptBasicAttack(ECombatCommand Command, FName ActionId);
    // 播放选定动作，并用指定事件结算被替换的实例。
    ECombatInputResult StartAction(ECombatCommand Command, FName ActionId, ECombatActionEvent PreviousEvent);
    // 再次确认当前实例储存的出招意图仍有效，然后尝试切招一次。
    bool ConsumeBufferedInput();

    // 接收指定动作实例的动画结束通知。
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId);
    // 结束给定实例并广播本次结算结果。
    void FinishAction(int64 InstanceId, ECombatActionEvent Event);
    // 分发一次动作生命周期通知。
    void NotifyAction(FName ActionId, int64 InstanceId, ECombatActionEvent Event);

    UPROPERTY(Transient)
    TObjectPtr<UCombatMotionComponent> Motion = nullptr;

    FName ActiveActionId = NAME_None;
    // 只在实例活动期间读取，区分同一指令的续段与高优先级技能打断。
    ECombatCommand ActiveCommand = ECombatCommand::BasicAttack;
    int64 ActiveInstanceId = 0;
    int64 LastInstanceId = 0;
    FName BufferedActionId = NAME_None;
    ECombatCommand BufferedCommand = ECombatCommand::BasicAttack;
    ECombatInputPhase BufferedPhase = ECombatInputPhase::Pressed;
    bool bEndingPlay = false;
};
