#pragma once

#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "NanamiFormComponent.generated.h"

class UCombatComponent;
class UCombatMotionComponent;
class UAnimSequence;
class USkeletalMeshComponent;

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
    // 仅攻击期间扣除游戏时间对应的星芒。
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    // 达到阈值只提交一次长按，直到下一次真实按下才重置。
    void OnHoldThreshold();
    // 跳至本实例的收刀首段，重复退出不会重播。
    void RequestYellowExit();
    // 监听成功起播和退出，提交形态变化。
    UFUNCTION()
    void OnActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event);
    // 在形态实际变化时通知动画和 HUD。
    void SetWeaponForm(ENanamiWeaponForm NewForm);
    // 用当前段的原始斧动画驱动独立武器，隐藏藏入斧中的剑盾几何。
    void UpdateAxePresentation(bool bVisible, FName Section = NAME_None);

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> Combat;
    UPROPERTY(Transient)
    TObjectPtr<UCombatMotionComponent> Motion;
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> BodyMesh;
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> AxeMesh;
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> PlayingAxeSequence;
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TObjectPtr<UAnimSequence> AxeIdle;
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TObjectPtr<UAnimSequence> AxeAttack44;
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TObjectPtr<UAnimSequence> AxeAttack47;
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<FName> SwordShieldBones;
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
    FTimerHandle HoldTimer;
};
