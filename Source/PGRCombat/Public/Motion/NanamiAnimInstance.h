#pragma once

#include "Animation/AnimInstance.h"
#include "NanamiAnimInstance.generated.h"

class UNanamiFormComponent;

UCLASS()
class PGRCOMBAT_API UNanamiAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // 装备动画蓝图从当前角色绑定形态源。
    void NativeInitializeAnimation() override;
    // 在游戏线程采样形态，由动画图选择相应待机。
    void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Form")
    bool bShieldAxe = false;

private:
    TWeakObjectPtr<UNanamiFormComponent> Form;
};
