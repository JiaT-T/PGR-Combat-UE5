#pragma once

#include "Engine/DataAsset.h"
#include "CombatActionSet.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct PGRCOMBAT_API FCombatActionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Animation")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, Category = "Animation")
    FName Section = NAME_None;

    UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.01"))
    float PlayRate = 1.0f;

    // 按动画本地时间从段末倒推，调播放速率时窗口仍对齐同一段姿势。
    UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.0", Units = "s"))
    float InputBufferSeconds = 0.0f;
};

UCLASS(BlueprintType)
class PGRCOMBAT_API UCombatActionSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Actions")
    TMap<FName, FCombatActionDefinition> Actions;
};
