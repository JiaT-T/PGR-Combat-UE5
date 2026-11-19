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
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ECombatInputResult SubmitInput(ECombatCommand Command, ECombatInputPhase Phase);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void InterruptActiveAction();

    UFUNCTION(BlueprintPure, Category = "Combat")
    FName GetActiveActionId() const { return ActiveActionId; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    int64 GetActiveInstanceId() const { return ActiveInstanceId; }

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FCombatActionChanged OnActionChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 InstanceId);
    void FinishAction(int64 InstanceId, ECombatActionEvent Event);
    void NotifyAction(FName ActionId, int64 InstanceId, ECombatActionEvent Event);

    UPROPERTY(Transient)
    TObjectPtr<UCombatMotionComponent> Motion = nullptr;

    FName ActiveActionId = NAME_None;
    int64 ActiveInstanceId = 0;
    int64 LastInstanceId = 0;
    bool bBasicAttackBuffered = false;
    bool bEndingPlay = false;
};
