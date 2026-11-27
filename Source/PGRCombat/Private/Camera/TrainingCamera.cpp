#include "Camera/TrainingCamera.h"

#include "Camera/CameraTypes.h"
#include "Combat/CombatComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"

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
    const double Lift = FMath::Max(0.0, Rise - GroundClearance) * 0.65;
    OutResult.Location.Z += Lift;

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
    const auto* Combat = Character->FindComponentByClass<UCombatComponent>();
    const bool bSmoothFraming = Combat && (Combat->IsDodging() || SmoothedFramingActions.Contains(Combat->GetActiveActionId()));
    if (bSmoothFraming || bSettlingFraming)
    {
        // 限制武器扫过时的镜头推拉，收势被下一招接走时也从当前距离继续跟随。
        const double Requested = Pullback;
        Pullback = FMath::FInterpConstantTo(LastPullback, Requested, DeltaTime, DodgeFramingSpeed);
        bSettlingFraming = bSmoothFraming || !FMath::IsNearlyEqual(Pullback, Requested);
    }
    LastPullback = Pullback;
    OutResult.Location -= Orientation.GetForwardVector() * Pullback;

    // 构图修正在弹簧臂避障之后发生，最终取景点也要与地面和场景保持相机探针的间距。
    const auto* Arm = Cast<USpringArmComponent>(GetOwner()->GetRootComponent()->GetAttachParent());
    if (Arm && Arm->bDoCollisionTest && (Pullback > 0.0 || Lift > 0.0))
    {
        const FVector Pivot = Arm->GetComponentLocation() + Arm->TargetOffset;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainingCameraFraming), false, Character);
        Query.AddIgnoredActor(GetOwner());
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit, Pivot, OutResult.Location, FQuat::Identity,
                                             Arm->ProbeChannel, FCollisionShape::MakeSphere(Arm->ProbeSize), Query))
        {
            OutResult.Location = Hit.Location;
        }
    }
}

// 保留 CameraActor 的视图目标和自动激活行为，使用训练场专用的取景组件。
ATrainingCamera::ATrainingCamera(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTrainingCameraComponent>(TEXT("CameraComponent")))
{
}
