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
    // 将输入组件的普攻事件连接到战斗逻辑。
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    // 角色离开世界时收回战斗输入。
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 撤销本角色先前注册的战斗输入映射。
    void RemoveCombatMapping();
    // 处理普攻按键起手。
    void OnBasicAttackPressed();
    // 处理普攻按键松开或取消。
    void OnBasicAttackReleased();

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UCombatComponent> Combat;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UCombatMotionComponent> Motion;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputMappingContext> CombatMapping;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
    TObjectPtr<UInputAction> BasicAttackAction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TObjectPtr<USkeletalMesh> FighterMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TSubclassOf<UAnimInstance> FighterAnimClass;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Presentation")
    TObjectPtr<UCombatActionSet> ActionSet;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};
