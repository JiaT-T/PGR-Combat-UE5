#include "Rendering/NanamiFaceLightingComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

// 在骨骼姿态提交后同步脸部材质，以便转身和连击时使用同一帧的头部方向。
void UNanamiFaceLightingComponent::BeginPlay()
{
    Super::BeginPlay();

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
    {
        return;
    }

    HeadIndex = Mesh->GetBoneIndex(TEXT("Bip001Head"));
    if (HeadIndex != INDEX_NONE)
    {
        RenderMesh = Mesh;
        PoseHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
            FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UNanamiFaceLightingComponent::UpdateFaceAxes));
        UpdateFaceAxes();
    }
}

// 解除网格上的回调，允许角色和渲染组件分别销毁。
void UNanamiFaceLightingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (USkeletalMeshComponent* Mesh = RenderMesh.Get())
    {
        Mesh->UnregisterOnBoneTransformsFinalizedDelegate(PoseHandle);
    }
    RenderMesh.Reset();
    Super::EndPlay(EndPlayReason);
}

// Stand2 对准结果表明源模型的面部 Y、Z 轴分别对应 UE 头骨局部的负 X、负 Z。
void UNanamiFaceLightingComponent::UpdateFaceAxes()
{
    if (USkeletalMeshComponent* Mesh = RenderMesh.Get())
    {
        const FQuat Rotation = Mesh->GetBoneTransform(HeadIndex).GetRotation();
        const FVector FaceY = -Rotation.GetAxisX();
        const FVector FaceZ = -Rotation.GetAxisZ();
        const float Axes[] = {
            float(FaceY.X), float(FaceY.Y), float(FaceY.Z), 0.0f,
            float(FaceZ.X), float(FaceZ.Y), float(FaceZ.Z)
        };
        Mesh->SetCustomPrimitiveDataFloatArray(0, MakeArrayView(Axes));
    }
}
