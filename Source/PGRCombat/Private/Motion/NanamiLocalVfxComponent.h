#pragma once

#include "Components/ActorComponent.h"
#include "NanamiLocalVfxComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UProceduralMeshComponent;
class USkeletalMeshComponent;

UCLASS()
class UNanamiLocalVfxComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 只对本机剑盾预览挂点安装特效播放组件。
    static void TrackMontage(USkeletalMeshComponent& Mesh, UAnimInstance& Anim, const UAnimMontage& Montage,
        FName ActionId, int32 InstanceId, float SectionStart);
    // 本地首段替换存在时抑制旧测试通知，其他场景保留旧行为。
    static bool ReplacesFirstHit(const USkeletalMeshComponent* Mesh, const UAnimSequenceBase* Animation);
    // 按当前蒙太奇实例触发挂点，并推进已经生成的独立特效。
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    struct FCue
    {
        FName Action;
        FString Clip;
        float Seconds = 0;
        FString Anchor;
        FVector Offset = FVector::ZeroVector;
        float Scale = 1;
        bool bActionBound = false;
    };

    struct FClip
    {
        TArray<FString> Layers;
        TArray<TArray<int32>> Offsets;
        TArray<uint8> Bytes;
    };

    struct FPlayback
    {
        const FClip* Clip = nullptr;
        FCue Cue;
        TArray<TWeakObjectPtr<UProceduralMeshComponent>> Layers;
        TArray<bool> Planted;
        FTransform Origin = FTransform::Identity;
        double Age = 0;
        int32 LastFrame = INDEX_NONE;
        int32 MontageId = INDEX_NONE;
    };

    // 读取只存于本机的时间线；文件缺失时保持原工程行为。
    bool ReadCues();
    // 读取几何快照并缓存帧索引。
    const FClip* GetClip(const FString& ClipName);
    // 将一条表现事件生成到世界、落地点或武器骨骼。
    void StartCue(const FCue& Cue, int32 MontageId, double Overshoot);
    // 根据挂点方式计算本次特效的局部到世界变换。
    FTransform GetOrigin(const FCue& Cue) const;
    // 刷新该次特效的可见网格，不参与角色的碰撞或根运动。
    void ApplyFrame(FPlayback& Effect, int32 Frame) const;
    // 回收本次播放创建的渲染组件。
    static void DestroyPlayback(FPlayback& Effect);

    // 从实际剑刃世界轨迹生成第四段拖尾，而不是重播预览相机的轨迹。
    void TickSwordTrail(float Time);
    void ResetSwordTrail();
    TWeakObjectPtr<UProceduralMeshComponent> SwordTrail;
    TArray<FTransform> SwordTrailPoses;
    TArray<float> SwordTrailTimes;
    int32 SwordTrailMontage = INDEX_NONE;
    bool bSwordTrailLogged = false;

    TArray<FCue> Cues;
    TArray<int32> Pending;
    TArray<FPlayback> Active;
    TMap<FString, TUniquePtr<FClip>> Clips;
    TWeakObjectPtr<USkeletalMeshComponent> CharacterMesh;
    TWeakObjectPtr<UAnimInstance> PlannedAnim;
    bool bYellowHoldExitSeen = true;
    int32 PlannedMontageId = INDEX_NONE;
    float PlannedSectionStart = 0;
    double YellowReturnStartPelvisZ = 0.0;
};
