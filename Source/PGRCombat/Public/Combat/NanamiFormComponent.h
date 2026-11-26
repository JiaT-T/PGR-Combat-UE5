#pragma once

#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "NanamiFormComponent.generated.h"

class UCombatComponent;
class UCombatMotionComponent;

UENUM(BlueprintType)
enum class ENanamiWeaponForm : uint8
{
    SwordShield,
    ShieldAxe
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNanamiFormChanged, ENanamiWeaponForm, Form);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PGRCOMBAT_API UNanamiFormComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 只在焚冲攻击段启用逐帧星芒消耗。
    UNanamiFormComponent();

    // 读取当前已经生效的武器形态。
    UFUNCTION(BlueprintPure, Category = "Form")
    ENanamiWeaponForm GetWeaponForm() const { return WeaponForm; }

    // 选招与 HUD 共用的形态许可，库存和二段资格由信号球组件管理。
    UFUNCTION(BlueprintPure, Category = "Form")
    bool CanUseRedBlueOrbs() const { return WeaponForm == ENanamiWeaponForm::SwordShield; }

    // 返回当前长按黄球所属的动作实例，动画蓝图按它区分收势与普通动作。
    int64 GetYellowHoldInstanceId() const { return HoldInstanceId; }

    // 判断焚冲是否正处于本次 Ability 配置的收势段，供逐帧姿势过渡使用。
    bool IsYellowHoldExitSection(FName Section) const;

    // 返回黄球和盾斧招式共用的星芒值。
    UFUNCTION(BlueprintPure, Category = "Form")
    float GetStarlight() const { return Starlight; }

    // 起手续轮必须重新按键，有残余时可再发动一轮。
    UFUNCTION(BlueprintPure, Category = "Form")
    bool CanStartYellowHold() const;

    // 支出实际拥有的星芒，耗尽时要求当前焚冲立即收刀。
    UFUNCTION(BlueprintCallable, Category = "Form")
    void ConsumeStarlight(float Amount);

    // 红蓝技能结算后补充星芒，取值限制在零至一百。
    void AddStarlight(float Amount);

    // 记录一次新的物理按下，并开始计长按阈值。
    void PressYellow();
    // 识别短按松手，或结束本次已经启动的长按。
    void ReleaseYellow();
    // 丢弃未成立的手势，也可结束已启动的攻击。
    void CancelYellow();

    UPROPERTY(BlueprintAssignable, Category = "Form")
    FNanamiFormChanged OnWeaponFormChanged;

protected:
    // 建立本角色战斗生命周期订阅并初始化训练星芒。
    void BeginPlay() override;
    // 清理输入计时和对本角色的订阅。
    void EndPlay(const EEndPlayReason::Type Reason) override;
    // 推进焚冲耗星和盾斧必杀的收势回切。
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    // 达到阈值只提交一次长按，直到下一次真实按下才重置。
    void OnHoldThreshold();
    // 从当前焚冲实例直接跳到回剑盾段，重复退出不会重播。
    void RequestYellowExit();
    // 监听成功起播和退出，提交形态变化。
    UFUNCTION()
    void OnActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event);
    // 在形态实际变化时通知动画和 HUD。
    void SetWeaponForm(ENanamiWeaponForm NewForm);

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> Combat;
    UPROPERTY(Transient)
    TObjectPtr<UCombatMotionComponent> Motion;
    UPROPERTY(EditDefaultsOnly, Category = "Training", meta = (ClampMin = "0", ClampMax = "100"))
    float InitialStarlight = 100.f;
    // 训练用输入阈值，单位为游戏秒。
    UPROPERTY(EditDefaultsOnly, Category = "Yellow Orb", meta = (ClampMin = "0.01", Units = "s"))
    float HoldThresholdSeconds = 0.30f;
    // 训练用星芒消耗速率，后续可直接用实测数值校正。
    UPROPERTY(EditDefaultsOnly, Category = "Yellow Orb", meta = (ClampMin = "0"))
    float StarlightDrainPerSecond = 12.f;

    ENanamiWeaponForm WeaponForm = ENanamiWeaponForm::SwordShield;
    float Starlight = 0.f;
    bool bCanSpendRemainder = false;
    bool bYellowPressed = false;
    bool bHoldThresholdReached = false;
    int64 HoldInstanceId = 0;
    // 只允许发动盾斧必杀的那个实例在收势时归还剑盾形态。
    int64 UltimateReturnInstanceId = 0;
    FTimerHandle HoldTimer;
};
