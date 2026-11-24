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
    // 在游戏线程采样移动和形态，由动画图选择基础姿态。
    void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Form")
    bool bShieldAxe = false;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement", meta = (Units = "cm/s"))
    float GroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement", meta = (Units = "Degrees"))
    float MoveDirection = 0.0f;

protected:
    // 在当帧 Section 确定后、动画图求值前，从上帧实际显示的姿势接入黄球收势。
    void Montage_Advance(float DeltaSeconds) override;

private:
    TWeakObjectPtr<UNanamiFormComponent> Form;
    // 松手和自然结束都从上一帧的攻击姿势混入同一个回剑盾段。
    UPROPERTY(EditDefaultsOnly, Category = "Form", meta = (ClampMin = "0", Units = "s"))
    float ReleasePoseBlendSeconds = 0.12f;
    int64 LastHoldInstanceId = 0;
    FName LastHoldSection = NAME_None;
};
