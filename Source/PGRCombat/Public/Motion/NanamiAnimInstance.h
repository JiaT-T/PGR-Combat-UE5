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

protected:
    // 在当帧 Section 确定后、动画图求值前，从上帧实际显示的姿势接入黄球收势。
    void Montage_Advance(float DeltaSeconds) override;

private:
    TWeakObjectPtr<UNanamiFormComponent> Form;
    // 手动跳至 48 和自然进入后续收势段共用同一姿势过渡时长。
    UPROPERTY(EditDefaultsOnly, Category = "Form", meta = (ClampMin = "0", Units = "s"))
    float ReleasePoseBlendSeconds = 0.12f;
    int64 LastHoldInstanceId = 0;
    FName LastHoldSection = NAME_None;
};
