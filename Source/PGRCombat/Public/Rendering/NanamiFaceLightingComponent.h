#pragma once

#include "Components/ActorComponent.h"
#include "NanamiFaceLightingComponent.generated.h"

class USkeletalMeshComponent;

// 将七实脸部材质所需的两个世界空间面轴写入角色网格。
UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class PGRCOMBAT_API UNanamiFaceLightingComponent : public UActorComponent
{
    GENERATED_BODY()

protected:
    // 在角色出场时订阅本帧骨骼姿态已完成的通知。
    virtual void BeginPlay() override;

    // 在角色离场时解除姿态订阅。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 从已更新的头骨旋转生成两个单位面轴并一次提交到材质。
    void UpdateFaceAxes();

    TWeakObjectPtr<USkeletalMeshComponent> RenderMesh;
    FDelegateHandle PoseHandle;
    int32 HeadIndex = INDEX_NONE;
};
