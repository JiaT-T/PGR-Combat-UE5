#include "Motion/NanamiAnimInstance.h"

#include "Combat/NanamiFormComponent.h"
#include "GameFramework/Actor.h"

// 动画实例重建时重新查找所属角色的形态组件。
void UNanamiAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    AActor* Owner = GetOwningActor();
    Form = Owner ? Owner->FindComponentByClass<UNanamiFormComponent>() : nullptr;
}

// 保持形态读取在游戏线程，供并行动画图使用该帧的只读姿态选择值。
void UNanamiAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    if (!Form.IsValid())
    {
        // 蓝图创建的组件可能晚于原生 Mesh 注册，首个有效角色帧再绑定。
        if (AActor* Owner = GetOwningActor())
        {
            Form = Owner->FindComponentByClass<UNanamiFormComponent>();
        }
    }
    bShieldAxe = Form.IsValid() && Form->GetWeaponForm() == ENanamiWeaponForm::ShieldAxe;
}
