#pragma once

#include "Engine/DataAsset.h"
#include "CombatActionSet.generated.h"

class UCombatMontageAbility;

UCLASS(BlueprintType)
class PGRCOMBAT_API UCombatActionSet : public UDataAsset
{
    GENERATED_BODY()

public:
    // 默认形态和形态共用的动作。每条 Ability 配置完整的播放与窗口数据。
    UPROPERTY(EditAnywhere, Category = "Actions")
    TMap<FName, TSubclassOf<UCombatMontageAbility>> Abilities;

    // 盾斧独有的表现沿用同一个动作标识，但可以使用与原 Montage 长度匹配的独立窗口。
    UPROPERTY(EditAnywhere, Category = "Actions")
    TMap<FName, TSubclassOf<UCombatMontageAbility>> ShieldAxeAbilities;
};
