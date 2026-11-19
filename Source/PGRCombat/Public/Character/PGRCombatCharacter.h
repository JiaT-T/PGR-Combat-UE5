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
    APGRCombatCharacter();
    void PawnClientRestart() override;
    void UnPossessed() override;

protected:
    void PostInitializeComponents() override;
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RemoveCombatMapping();
    void OnBasicAttackPressed();
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
