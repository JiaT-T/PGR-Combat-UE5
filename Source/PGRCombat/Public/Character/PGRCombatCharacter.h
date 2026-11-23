#pragma once

#include "GameFramework/Character.h"
#include "PGRCombatCharacter.generated.h"

class UCombatActionSet;
class UCombatComponent;
class UCombatMotionComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

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

protected:
    // 在组件就绪后应用角色的表现配置。
    void PostInitializeComponents() override;
    // 将普攻和红、蓝球的按键事件连接到战斗逻辑。
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    // 角色离开世界时收回战斗输入。
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 撤销本角色先前注册的战斗输入映射。
    void RemoveCombatMapping();
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
