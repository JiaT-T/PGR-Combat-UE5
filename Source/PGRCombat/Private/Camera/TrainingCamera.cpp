#include "Camera/TrainingCamera.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

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
    ApplyAttack99CameraPreview(*Character, OutResult, DeltaTime);
}

// 保留 CameraActor 的视图目标和自动激活行为，使用训练场专用的取景组件。
ATrainingCamera::ATrainingCamera(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTrainingCameraComponent>(TEXT("CameraComponent")))
{
}

namespace
{
    struct FAttack99PreviewKey
    {
        float Time;
        FVector LocalPosition;
        FQuat LocalRotation;
        float VerticalFov;
    };

    // 优先使用本地验收曲线，常规启动从开发包内读取轨迹；保留回退开关做对照。
    TArray<FAttack99PreviewKey> LoadAttack99Preview()
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("PGRDisableAttack99Camera")))
        {
            return {};
        }
        FString Path;
        if (!FParse::Value(FCommandLine::Get(), TEXT("PGRPreviewAttack99Camera="), Path))
        {
            Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Camera/Nanami/Attack99.csv"));
        }
        TArray<FString> Lines;
        TArray<FAttack99PreviewKey> Keys;
        if (!FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.Num() < 3)
        {
            UE_LOG(LogTemp, Warning, TEXT("PGR_CAMERA99 missing curve file; using ordinary follow camera"));
            return {};
        }
        for (int32 Index = 1; Index < Lines.Num(); ++Index)
        {
            TArray<FString> Cells;
            Lines[Index].ParseIntoArray(Cells, TEXT(","), false);
            double V[9];
            if (Cells.Num() != 10)
            {
                return {};
            }
            for (int32 Col = 0; Col < 9; ++Col)
            {
                if (!LexTryParseString(V[Col], *Cells[Col + 1]) || !FMath::IsFinite(V[Col]))
                {
                    return {};
                }
            }
            if (V[0] < 0 || (!Keys.IsEmpty() && V[0] <= Keys.Last().Time) || V[8] <= 1 || V[8] >= 175)
            {
                return {};
            }
            // 按 Unity 的右、上、前轴转到 UE，并将距离换算为厘米。
            const double VFov = V[8];
            const FQuat Rotation(V[6], V[4], V[5], V[7]);
            if (Rotation.SizeSquared() < 0.8 || Rotation.SizeSquared() > 1.2)
            {
                return {};
            }
            Keys.Add({float(V[0]), FVector(V[3], V[1], V[2]) * 100.0, Rotation.GetNormalized(), float(VFov)});
        }
        UE_LOG(LogTemp, Display, TEXT("PGR_CAMERA99 loaded %d verified samples"), Keys.Num());
        return Keys;
    }

    // 腾空时混合特殊镜头，落地和收势仍由普通跟随镜头取景。
    float Attack99PreviewWeight(float Time)
    {
        const float Enter = FMath::SmoothStep(1.16f, 1.43f, Time);
        const float Leave = 1.f - FMath::SmoothStep(3.38f, 3.67f, Time);
        return Enter * Leave;
    }
}

// 按当前 Attack99 实例采样混合，只改变画面，不修改动作和技能状态。
void UTrainingCameraComponent::ApplyAttack99CameraPreview(const ACharacter& Character, FMinimalViewInfo& View, float DeltaTime)
{
    static const TArray<FAttack99PreviewKey> Keys = LoadAttack99Preview();
    if (Keys.IsEmpty())
    {
        return;
    }
    const UCombatComponent* Combat = Character.FindComponentByClass<UCombatComponent>();
    const USkeletalMeshComponent* Mesh = Character.GetMesh();
    const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    const FAnimMontageInstance* Montage = Anim ? Anim->GetActiveMontageInstance() : nullptr;
    const bool bTarget = Combat && Combat->GetActiveActionId() == FName(TEXT("Attack99"))
        && Combat->GetActiveInstanceId() != 0 && Montage;
    if (!bTarget && CameraPreviewInstance == 0)
    {
        return;
    }
    const int64 CurrentInstance = bTarget ? Combat->GetActiveInstanceId() : 0;
    const float Time = bTarget ? Montage->GetPosition() : 0.f;
    if (bTarget && CurrentInstance != CameraPreviewInstance)
    {
        CameraPreviewInstance = CurrentInstance;
        CameraPreviewStart = Character.GetActorLocation() - FVector(0.0, 0.0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        CameraPreviewHeading = FRotator(0.0, Character.GetActorRotation().Yaw, 0.0).Quaternion();
        CameraPreviewLastWeight = 0.f;
        bCameraPreviewLogged = false;
    }
    if (bTarget)
    {
        const float Weight = Attack99PreviewWeight(Time);
        if (Weight <= 0.f)
        {
            if (CameraPreviewLastWeight > 0.f)
            {
                UE_LOG(LogTemp, Display, TEXT("PGR_CAMERA99 follows again at montage %.3f"), Time);
            }
            CameraPreviewLastWeight = 0.f;
            return;
        }
        if (!bCameraPreviewLogged)
        {
            UE_LOG(LogTemp, Display, TEXT("PGR_CAMERA99 starts instance %lld at montage %.3f"), CurrentInstance, Time);
            bCameraPreviewLogged = true;
        }
        int32 Index = FMath::Clamp(FMath::FloorToInt(Time * 30.f), 0, Keys.Num() - 2);
        while (Index > 0 && Keys[Index].Time > Time) { --Index; }
        while (Index < Keys.Num() - 2 && Keys[Index + 1].Time < Time) { ++Index; }
        const FAttack99PreviewKey& A = Keys[Index];
        const FAttack99PreviewKey& B = Keys[Index + 1];
        const float T = FMath::Clamp((Time - A.Time) / (B.Time - A.Time), 0.f, 1.f);
        CameraPreviewPosition = CameraPreviewStart + CameraPreviewHeading.RotateVector(FMath::Lerp(A.LocalPosition, B.LocalPosition, T));
        CameraPreviewRotation = CameraPreviewHeading * FQuat::Slerp(A.LocalRotation, B.LocalRotation, T);
        int32 Width = 0, Height = 0;
        if (const APlayerController* Player = Cast<APlayerController>(Character.GetController()))
        {
            Player->GetViewportSize(Width, Height);
        }
        const double Ratio = View.bConstrainAspectRatio || Height <= 0
            ? FMath::Max(0.01, double(View.AspectRatio)) : double(Width) / Height;
        const float Vertical = FMath::Lerp(A.VerticalFov, B.VerticalFov, T);
        CameraPreviewHorizontalFov = FMath::RadiansToDegrees(2.0 * FMath::Atan(Ratio * FMath::Tan(FMath::DegreesToRadians(Vertical * 0.5))));
        CameraPreviewLastWeight = Weight;
    }
    else
    {
        CameraPreviewLastWeight = FMath::Max(0.f, CameraPreviewLastWeight - DeltaTime / 0.18f);
        if (CameraPreviewLastWeight <= 0.f)
        {
            CameraPreviewInstance = 0;
            return;
        }
    }

    View.Location = FMath::Lerp(View.Location, CameraPreviewPosition, CameraPreviewLastWeight);
    View.Rotation = FQuat::Slerp(View.Rotation.Quaternion(), CameraPreviewRotation, CameraPreviewLastWeight).Rotator();
    View.FOV = FMath::Lerp(View.FOV, CameraPreviewHorizontalFov, CameraPreviewLastWeight);
    View.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
}
