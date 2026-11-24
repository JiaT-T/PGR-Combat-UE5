#pragma once

#include "GameFramework/Character.h"
#include "PGRCombatCharacter.generated.h"

class UCombatActionSet;
class UCombatComponent;
class UCombatMotionComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
enum class ECombatActionEvent : uint8;
struct FInputActionValue;

UCLASS()
class PGRCOMBAT_API APGRCombatCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    // 构造可由玩家控制的战斗角色。
    APGRCombatCharacter();
    // 在本地玩家重新接管角色时恢复战斗输入。
    void PawnClientRestart() override;
    // 失去控制权时释放本角色占用的输入映射。
    void UnPossessed() override;
    // 全程锁定的技能中忽略玩家平移输入，动画自身根位移照常生效。
    bool IsMoveInputIgnored() const override;

protected:
    // 在组件就绪后应用角色的表现配置。
    void PostInitializeComponents() override;
    // 在同一个输入组件上绑定基础移动和战斗指令。
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    // 角色离开世界时收回战斗输入。
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 撤销本角色先前注册的战斗输入映射。
    void RemoveCombatMapping();
    // 将本帧二维输入转换为训练镜头朝向下的移动意图。
    void OnMove(const FInputActionValue& Value);
    // 让成功起播的动作接管移动和朝向，动作结算后恢复移动转向。
    UFUNCTION()
    void OnCombatActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event);
    // 处理普攻按键起手。
    void OnBasicAttackPressed();
    // 处理已经触发的普攻输入结束。
    void OnBasicAttackReleased();
    // 处理触发条件尚未满足就结束的普攻输入。
    void OnBasicAttackCanceled();
    // 提交一次红球按下指令。
    void OnRedOrbPressed();
    // 提交一次蓝球按下输入。
    void OnBlueOrbPressed();

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UCombatComponent> Combat;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UCombatMotionComponent> Motion;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputMappingContext> CombatMapping;

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputAction> BasicAttackAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputAction> RedOrbAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputAction> BlueOrbAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputAction> YellowOrbAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TObjectPtr<USkeletalMesh> FighterMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TSubclassOf<UAnimInstance> FighterAnimClass;

    // 修正导入骨架与显示网格叠加的长度缩放，只影响胶囊体接收的动画位移。
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation", meta = (ClampMin = "0.0"))
    float FighterRootMotionTranslationScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TObjectPtr<UCombatActionSet> ActionSet;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};
