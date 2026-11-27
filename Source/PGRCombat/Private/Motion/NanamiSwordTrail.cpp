#include "Motion/NanamiLocalVfxComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

// 清理局部拖尾，蒙太奇切换时不把旧坐标传给下一招。
void UNanamiLocalVfxComponent::ResetSwordTrail()
{
    if (auto* Trail = SwordTrail.Get())
    {
        Trail->DestroyComponent();
    }
    SwordTrail.Reset();
    SwordTrailPoses.Reset();
    SwordTrailTimes.Reset();
    SwordTrailMontage = INDEX_NONE;
    bSwordTrailLogged = false;
}

// 按已验证的原剑材质和剑刃宽度，把最近一小段真实动作扫成三维拖尾。
void UNanamiLocalVfxComponent::TickSwordTrail(float Time)
{
    constexpr float Start = 5.f / 60.f;
    constexpr float Stop = 48.f / 60.f;
    constexpr float Lifetime = .30f;
    const FName Bone(TEXT("Blade_WeaponBone1"));
    const auto* Mesh = CharacterMesh.Get();
    if (Time >= Stop + Lifetime)
    {
        ResetSwordTrail();
        return;
    }
    if (!Mesh || Time < Start || Mesh->GetBoneIndex(Bone) == INDEX_NONE)
    {
        return;
    }
    if (Time <= Stop && (SwordTrailTimes.IsEmpty() || Time > SwordTrailTimes.Last()))
    {
        SwordTrailTimes.Add(Time);
        SwordTrailPoses.Add(Mesh->GetSocketTransform(Bone, RTS_World));
    }
    while (!SwordTrailTimes.IsEmpty() && Time - SwordTrailTimes[0] >= Lifetime)
    {
        SwordTrailTimes.RemoveAt(0);
        SwordTrailPoses.RemoveAt(0);
    }
    if (SwordTrailTimes.Num() < 2)
    {
        if (SwordTrail.IsValid()) SwordTrail->SetVisibility(false);
        return;
    }
    if (!SwordTrail.IsValid())
    {
        auto* Material=LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ActionTrail/M_Trail02.M_Trail02"));
        if (!Material)
        {
            ResetSwordTrail();
            return;
        }
        auto* Trail=NewObject<UProceduralMeshComponent>(GetOwner());
        Trail->SetMobility(EComponentMobility::Movable);
        Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Trail->SetCastShadow(false);
        Trail->SetTranslucentSortPriority(16);
        Trail->SetMaterial(0,Material);
        GetOwner()->AddInstanceComponent(Trail);
        Trail->RegisterComponent();
        Trail->SetWorldTransform(FTransform::Identity);
        SwordTrail=Trail;
    }
    const float Widths[]{0.f,22.04859f,58.95471f,85.81714f};
    TArray<float> Arc{0};
    for (int32 I=1; I<SwordTrailPoses.Num(); ++I)
    {
        const FVector Tip(0,65,0);
        Arc.Add(Arc.Last()+FVector::Dist(SwordTrailPoses[I].TransformPositionNoScale(Tip),
            SwordTrailPoses[I-1].TransformPositionNoScale(Tip)));
    }
    const float Length = Arc.Last();
    if (Length < 1.f)
    {
        SwordTrail->SetVisibility(false);
        return;
    }
    TArray<FVector> V;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Color;
    TArray<int32> Tri;
    for (int32 I=0;I<SwordTrailPoses.Num();++I)
    {
        const float Opacity = FMath::Clamp(1.f-(Time-SwordTrailTimes[I])/Lifetime,0.f,1.f)*
            FMath::Clamp((SwordTrailTimes[I]-Start)*20.f,0.f,1.f);
        for(int32 J=0;J<UE_ARRAY_COUNT(Widths);++J)
        {
            V.Add(SwordTrailPoses[I].TransformPositionNoScale(FVector(0,Widths[J],0)));
            UV.Emplace(Arc[I]/Length,Widths[J]/Widths[3]);
            Color.Emplace(1,1,1,Opacity);
            if (I>0 && J>0)
            {
                const int32 K=V.Num()-1;
                Tri.Append({K-5,K-4,K,K-5,K,K-1});
            }
        }
    }
    SwordTrail->CreateMeshSection_LinearColor(0,V,Tri,{},UV,Color,{},false,false);
    SwordTrail->SetVisibility(true);
    if (!bSwordTrailLogged && Time >= .17f)
    {
        bSwordTrailLogged = true;
        UE_LOG(LogTemp,Display,TEXT("PGR_SWORD04_TRAIL_VISIBLE time=%.3f vertices=%d length=%.1f"),Time,V.Num(),Length);
    }
}
