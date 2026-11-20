#pragma once

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "TrainingCamera.generated.h"

UCLASS()
class PGRCOMBAT_API UTrainingCameraComponent : public UCameraComponent
{
    GENERATED_BODY()

public:
    // 在原有弹簧臂构图上，为挂接角色的腾空姿势和武器留出画面空间。
    virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& OutResult) override;

private:
    UPROPERTY(EditInstanceOnly, Category = "Framing")
    FName FocusBone;

    // 取景点以骨骼局部坐标记录，伸缩武器的轮廓端点随骨骼缩放一起移动。
    UPROPERTY(EditInstanceOnly, Category = "Framing")
    TMap<FName, FVector> FramingPoints;

    // 骨盆超过胶囊中心的这段距离后镜头才抬升，地面动作沿用训练场构图。
    UPROPERTY(EditInstanceOnly, Category = "Framing", meta = (ClampMin = "0", Units = "cm"))
    float GroundClearance = 65.0f;

    // 骨点到模型和武器外沿仍有距离，拟合画面时为这部分留白。
    UPROPERTY(EditInstanceOnly, Category = "Framing", meta = (ClampMin = "0", Units = "cm"))
    float FramingPadding = 45.0f;
};

UCLASS()
class PGRCOMBAT_API ATrainingCamera : public ACameraActor
{
    GENERATED_BODY()

public:
    // 让玩家相机管理器通过训练场取景组件读取该相机的视图。
    ATrainingCamera(const FObjectInitializer& ObjectInitializer);
};
