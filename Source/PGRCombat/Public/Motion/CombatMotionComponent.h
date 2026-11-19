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

    bool PlayAction(FName ActionId, FOnMontageEnded OnEnded);
    bool IsInputBufferOpen() const;
    void StopAction();

private:
    TWeakObjectPtr<UAnimInstance> PlayingAnimInstance;
    int32 MontageInstanceId = INDEX_NONE;
    float BufferStart = 0.0f;
    float BufferEnd = 0.0f;
};
