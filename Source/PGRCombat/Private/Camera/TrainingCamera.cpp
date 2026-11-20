#include "Camera/TrainingCamera.h"

#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

// 抬升腾空角色的取景中心，再计算容纳头、脚及武器端点所需的最短后拉距离。
void UTrainingCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::GetCameraView(DeltaTime, OutResult);

    const ACharacter* Character = Cast<ACharacter>(GetOwner()->GetAttachParentActor());
    const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
    if (!Mesh || !Mesh->DoesSocketExist(FocusBone))
    {
        return;
    }

    const double Rise = Mesh->GetSocketLocation(FocusBone).Z - Character->GetActorLocation().Z;
    OutResult.Location.Z += FMath::Max(0.0, Rise - GroundClearance) * 0.65;

    const FQuat Orientation = OutResult.Rotation.Quaternion();
    const double TanHorizontal = FMath::Tan(FMath::DegreesToRadians(OutResult.FOV * 0.5));
    const double TanVertical = TanHorizontal / FMath::Max(0.01f, OutResult.AspectRatio);
    double Pullback = 0.0;
    for (const TPair<FName, FVector>& Marker : FramingPoints)
    {
        if (Mesh->DoesSocketExist(Marker.Key))
        {
            const FVector WorldPoint = Mesh->GetSocketTransform(Marker.Key).TransformPosition(Marker.Value);
            const FVector Point = Orientation.UnrotateVector(WorldPoint - OutResult.Location);
            const double RequiredDepth = FMath::Max((FMath::Abs(Point.Y) + FramingPadding) / TanHorizontal,
                                                     (FMath::Abs(Point.Z) + FramingPadding) / TanVertical);
            Pullback = FMath::Max(Pullback, RequiredDepth - Point.X);
        }
    }
    OutResult.Location -= Orientation.GetForwardVector() * Pullback;
}

// 保留 CameraActor 的视图目标和自动激活行为，使用训练场专用的取景组件。
ATrainingCamera::ATrainingCamera(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTrainingCameraComponent>(TEXT("CameraComponent")))
{
}
