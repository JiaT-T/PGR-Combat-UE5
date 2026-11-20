#include "Camera/TrainingPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

// 视角输入由训练关卡的控制器处理，角色仍由自己的 Enhanced Input 映射接收普攻。
void ATrainingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAxisKey(EKeys::MouseX, this, &ThisClass::OrbitYaw);
    InputComponent->BindAxisKey(EKeys::MouseY, this, &ThisClass::OrbitPitch);
    InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ThisClass::Zoom);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ThisClass::Zoom);
}

// 等关卡完成出生朝向设置后再初始化视角，重新接管时也走同一路径。
void ATrainingPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    bInitializeOrbit = true;
}

// 首帧读取弹簧臂的布置角度，随后由相机管理器处理每帧旋转和俯仰限制。
void ATrainingPlayerController::UpdateRotation(float DeltaTime)
{
    if (bInitializeOrbit && GetPawn() && PlayerCameraManager)
    {
        if (const auto* Arm = GetPawn()->FindComponentByClass<USpringArmComponent>())
        {
            SetControlRotation(Arm->GetComponentRotation());
        }
        PlayerCameraManager->ViewPitchMin = -60.0f;
        // 给贴地仰视留出下沿空间，在宽屏训练场也能看见站姿的脚部。
        PlayerCameraManager->ViewPitchMax = 10.0f;
        bInitializeOrbit = false;
    }
    Super::UpdateRotation(DeltaTime);
}

// 鼠标报告的是本帧的位移，直接累加以保持不同帧率下的转向距离一致。
void ATrainingPlayerController::OrbitYaw(float Delta)
{
    if (!IsLookInputIgnored())
    {
        RotationInput.Yaw += Delta * MouseSensitivity;
    }
}

// 根据垂直鼠标位移抬高或压低观察方向。
void ATrainingPlayerController::OrbitPitch(float Delta)
{
    if (!IsLookInputIgnored())
    {
        // MouseY 向上为正，训练镜头使用与水平方向相同的灵敏度。
        RotationInput.Pitch += Delta * MouseSensitivity;
    }
}

// 每个滚轮刻度只改变观察距离，腾空动作继续使用原有的自动取景。
void ATrainingPlayerController::Zoom(FKey Key)
{
    const APawn* ControlledPawn = GetPawn();
    auto* Arm = ControlledPawn ? ControlledPawn->FindComponentByClass<USpringArmComponent>() : nullptr;
    if (Arm)
    {
        const float Step = Key == EKeys::MouseScrollUp ? -ZoomStep : ZoomStep;
        Arm->TargetArmLength = FMath::Clamp(Arm->TargetArmLength + Step, float(ZoomRange.X), float(ZoomRange.Y));
    }
}
