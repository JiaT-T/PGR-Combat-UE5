#pragma once

#include "GameFramework/PlayerController.h"
#include "TrainingPlayerController.generated.h"

UCLASS()
class PGRCOMBAT_API ATrainingPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    // 配置训练镜头的鼠标转向与滚轮输入。
    virtual void SetupInputComponent() override;
    // 重新接管角色时请求恢复关卡布置的初始视角。
    virtual void OnPossess(APawn* InPawn) override;
    // 在出生朝向设置完成后初始化镜头，再应用当帧鼠标输入。
    virtual void UpdateRotation(float DeltaTime) override;

private:
    // 将鼠标水平位移转换为绕角色的偏航角。
    void OrbitYaw(float Delta);
    // 将鼠标垂直位移转换为受相机管理器限制的俯仰角。
    void OrbitPitch(float Delta);
    // 在训练镜头允许的距离内拉近或推远弹簧臂。
    void Zoom(FKey Key);

    UPROPERTY(EditDefaultsOnly, Category = "Training|Camera", meta = (ClampMin = "0"))
    float MouseSensitivity = 3.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Camera", meta = (Units = "cm"))
    FVector2D ZoomRange = FVector2D(320.0, 950.0);

    UPROPERTY(EditDefaultsOnly, Category = "Training|Camera", meta = (Units = "cm", ClampMin = "0"))
    float ZoomStep = 65.0f;

    bool bInitializeOrbit = true;
};
