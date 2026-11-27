#include "NanamiLocalVfxComponent.h"
#include "Motion/CombatMotionComponent.h"
#include "Combat/CombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/MemoryReader.h"

DEFINE_LOG_CATEGORY_STATIC(LogNanamiLocalVfx, Log, All);

namespace
{
    const FName WeaponAnchor(TEXT("Bip001Prop2"));
    const FName PelvisAnchor(TEXT("Bip001Pelvis"));

    // 返回不随仓库分发的特效快照目录。
    FString DataPath(const FString& Filename)
    {
        return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("LocalOnly/NanamiVfx/Data"), Filename);
    }

    // 标记蓝球白色线芯的可见脉冲帧，覆盖低帧率跳过单帧的情况。
    bool IsBoltPulse(const FString& Layer, int32 Frame)
    {
        return Frame == 20 || Frame == 22 || (Layer == TEXT("L08_Lei01")
            ? Frame == 33 || Frame == 40 : Frame == 31 || Frame == 34 || Frame == 45);
    }

    // 多段斩击以各自的出手点锁定世界位置，避免第二刀仍留在起跑点。
    int32 WorldPlantFrame(const FString& Clip, const FString& Layer)
    {
        // 每道扫击只在出光前跟随当时朝向，之后留在世界坐标，避免把预烘焙挥斧再旋转一次。
        const int32 AxeLayer = Layer.StartsWith(TEXT("A6")) ? FCString::Atoi(*Layer.Mid(5, 2)) : INDEX_NONE;
        if (Clip == TEXT("Attack61_Ex") || Clip == TEXT("Attack64_Ex") || Clip == TEXT("Attack64_ExImpact")) return 2;
        if (Clip == TEXT("Attack62_Ex")) return AxeLayer < 13 ? 2 : 38;
        if (Clip == TEXT("Attack62_ExImpact")) return AxeLayer < 40 ? 2 : 38;
        if (Clip == TEXT("Attack63_Ex")) return AxeLayer < 14 ? 10 : AxeLayer < 26 ? 25 : 46;
        if (Clip == TEXT("Attack61") || Clip == TEXT("Attack64")) return 2;
        if (Clip == TEXT("Attack62")) return AxeLayer < 8 ? 2 : 38;
        if (Clip == TEXT("Attack62_Impact")) return AxeLayer < 26 ? 2 : 38;
        if (Clip == TEXT("Attack63")) return AxeLayer < 8 ? 10 : AxeLayer < 16 ? 25 : 46;
        if (Clip == TEXT("Attack01")) return Layer.StartsWith(TEXT("Qiliu")) ? 2 : 8;
        if (Clip == TEXT("Attack02")) return Layer.StartsWith(TEXT("G2")) ? 25 : 8;
        if (Clip == TEXT("Attack03"))
        {
            const int32 ID = FCString::Atoi(*Layer.Mid(1, 2));
            return ID >= 15 ? 38 : ID >= 10 ? 16 : ID == 1 ? 2 : 8;
        }
        if (Clip == TEXT("Attack04")) return Layer.Contains(TEXT("Qiliu")) ? 38 : 50;
        if (Clip == TEXT("Attack11")) return FCString::Atoi(*Layer.Mid(1, 2)) < 8 ? 15 : 2;
        if (Clip == TEXT("Attack22")) return Layer.Contains(TEXT("Qiliu")) ? 2 : 16;
        // 变形的两道扫光各自在出场时锁定，避免后一道落在动作起手处。
        if (Clip == TEXT("Attack31"))
        {
            const int32 Id = FCString::Atoi(*Layer.Mid(1, 2));
            return Id <= 11 || Id == 30 ? 30 : 2;
        }
        if (Clip == TEXT("Attack31_Weapon")) return 14;
        if (Clip == TEXT("Attack32")) return 2;
        return 0;
    }

    // 复用独立预览已有的淡出系数，不改粒子层的材质映射。
    void SetMaterialFrame(UMaterialInstanceDynamic& Material, const FString& Clip, const FString& Layer, int32 Frame)
    {
        Material.SetScalarParameterValue(TEXT("FxTime"), Frame / 60.0f);
        if (Clip == TEXT("Attack01"))
        {
            static const TMap<FString, FVector4> Fade = {
                {TEXT("An"), FVector4(15, 21, 1, 1.5)}, {TEXT("An1"), FVector4(16, 22, 1, 1.3)},
                {TEXT("Glow"), FVector4(14, 21, 1, 1.4)}, {TEXT("Qiliu"), FVector4(7, 19, .96, 2)},
                {TEXT("Qiliu2A"), FVector4(12, 19, .88, 1.2)}};
            if (const FVector4* Q = Fade.Find(Layer))
            {
                const float T = FMath::Clamp((Frame - Q->X) / (Q->Y - Q->X), 0.0, 1.0);
                Material.SetScalarParameterValue(TEXT("FrameFade"), Q->Z * FMath::Pow(1.0f - T, Q->W));
            }
        }
        else if (Clip == TEXT("Attack03_Blade"))
        {
            const float Fade = (Layer == TEXT("A101_L04_Ring") || Layer == TEXT("A101_L05_Line") ||
                Layer == TEXT("A101_L06_LineA")) ? .14f :
                (Layer == TEXT("A101_L08_Par") || Layer == TEXT("A101_L09_Suduxian")) ? .19f :
                (Layer == TEXT("A101_L02_Lighitng") || Layer == TEXT("A101_L03_Path") || Layer == TEXT("A101_L10_Path")) ? .32f :
                Layer == TEXT("A101_L07_wave") ? .30f : 1.f;
            Material.SetScalarParameterValue(TEXT("FrameFade"), Fade);
        }
        else if (Clip == TEXT("Attack03_Air"))
        {
            Material.SetScalarParameterValue(TEXT("FrameFade"), .37f);
        }
        else if (Clip == TEXT("Attack02") && Layer == TEXT("G2_An"))
        {
            const float Tail = FMath::SmoothStep(34.f, 39.f, float(Frame)) * (1.f - FMath::SmoothStep(41.f, 44.f, float(Frame)));
            Material.SetScalarParameterValue(TEXT("Tail"), Tail);
            Material.SetScalarParameterValue(TEXT("DetailStrength"), 1.f);
        }
    }
}

// 仅在匹配七实剑盾蒙太奇的本机训练中启动表现时间线，不改动作规则。
void UNanamiLocalVfxComponent::TrackMontage(USkeletalMeshComponent& Mesh, UAnimInstance& Anim,
    const UAnimMontage& Montage, FName ActionId, int32 InstanceId, float SectionStart)
{
    AActor* Owner = Mesh.GetOwner();
    const FString ExpectedMontage =
        ActionId == TEXT("YellowSwordToAxe") ? TEXT("AM_YellowTapSwordToAxe") :
        ActionId == TEXT("YellowAxeToSword") ? TEXT("AM_YellowTapAxeToSword") :
        TEXT("AM_Equipped") + ActionId.ToString();
    if (!Owner || !Mesh.GetWorld() || !Mesh.GetWorld()->IsGameWorld() || Mesh.GetWorld()->GetNetMode() == NM_DedicatedServer ||
        (Montage.GetName() != ExpectedMontage &&
         !(ActionId == TEXT("DodgeBack") && (Montage.GetFName() == TEXT("AM_SwordDodgeBack") || Montage.GetFName() == TEXT("AM_AxeDodgeBack"))) &&
         !(ActionId == TEXT("DodgeTravel") && (Montage.GetFName() == TEXT("AM_SwordDodgeTravel") || Montage.GetFName() == TEXT("AM_AxeDodgeTravel")))))
    {
        return;
    }
    auto* Vfx = Owner->FindComponentByClass<UNanamiLocalVfxComponent>();
    if (!Vfx)
    {
        Vfx = NewObject<UNanamiLocalVfxComponent>(Owner);
        if (!Vfx->ReadCues())
        {
            return;
        }
        Vfx->CharacterMesh = &Mesh;
        Vfx->PrimaryComponentTick.bCanEverTick = true;
        Vfx->PrimaryComponentTick.TickGroup = TG_PostPhysics;
        Owner->AddInstanceComponent(Vfx);
        Vfx->RegisterComponent();
        Vfx->AddTickPrerequisiteComponent(&Mesh);
    }
    if (ActionId == TEXT("YellowAxeToSword") && Mesh.GetBoneIndex(PelvisAnchor) != INDEX_NONE)
    {
        Vfx->YellowReturnStartPelvisZ = Mesh.GetSocketTransform(PelvisAnchor, RTS_Component).GetLocation().Z;
    }
    Vfx->ResetSwordTrail();
    if (ActionId == TEXT("Attack04"))
    {
        Vfx->SwordTrailMontage = InstanceId;
    }
    Vfx->Pending.Reset();
    Vfx->bYellowHoldExitSeen = ActionId != TEXT("YellowHold");
    for (int32 I = 0; I < Vfx->Cues.Num(); ++I)
    {
        if (Vfx->Cues[I].Action == ActionId)
        {
            Vfx->Pending.Add(I);
        }
    }
    if (!Vfx->Pending.IsEmpty())
    {
        Vfx->PlannedAnim = &Anim;
        Vfx->PlannedMontageId = InstanceId;
        Vfx->PlannedSectionStart = SectionStart;
        Vfx->SetComponentTickEnabled(true);
    }
}

// 只在新首段已被当前角色接管时拦截旧 Niagara，避免同一刀出现两套。
bool UNanamiLocalVfxComponent::ReplacesFirstHit(const USkeletalMeshComponent* Mesh, const UAnimSequenceBase* Animation)
{
    const AActor* Owner = Mesh ? Mesh->GetOwner() : nullptr;
    const auto* Vfx = Owner ? Owner->FindComponentByClass<UNanamiLocalVfxComponent>() : nullptr;
    return Vfx && Animation && Animation->GetFName() == FName(TEXT("AM_EquippedAttack01")) &&
        Vfx->Cues.ContainsByPredicate([](const FCue& Cue) { return Cue.Clip == TEXT("Attack01"); }) &&
        FPaths::FileExists(DataPath(TEXT("Attack01.pgrfx")));
}

// 加载本机预览 cue，允许按录像微调时间和挂点而无需重建角色资产。
bool UNanamiLocalVfxComponent::ReadCues()
{
    TArray<FString> Lines;
    const TCHAR* CueFile = FParse::Param(FCommandLine::Get(), TEXT("AxeBaseVfx")) ? TEXT("cues-base.csv") : TEXT("cues.csv");
    if (!FFileHelper::LoadFileToStringArray(Lines, *DataPath(CueFile)))
    {
        return false;
    }
    for (const FString& Line : Lines)
    {
        TArray<FString> Parts;
        Line.ParseIntoArray(Parts, TEXT(","), false);
        if (Parts.Num() != 9 || !Parts[1].StartsWith(TEXT("Attack")) || !FPaths::IsRelative(Parts[1]) || Parts[1].Contains(TEXT("/")) || Parts[1].Contains(TEXT("\\")) || Parts[1].Contains(TEXT(".")) || !Parts[2].IsNumeric())
        {
            continue;
        }
        FCue& Cue = Cues.AddDefaulted_GetRef();
        Cue.Action = FName(*Parts[0]);
        Cue.Clip = Parts[1];
        Cue.Seconds = FMath::Max(0.f, FCString::Atof(*Parts[2]));
        Cue.Anchor = Parts[3];
        Cue.Offset = FVector(FCString::Atod(*Parts[4]), FCString::Atod(*Parts[5]), FCString::Atod(*Parts[6]));
        Cue.Scale = FMath::Clamp(FCString::Atof(*Parts[7]), 0.05f, 5.f);
        Cue.bActionBound = Parts[8] == TEXT("Action");
    }
    return !Cues.IsEmpty();
}

// 索引本地打包的网格；缺失或不匹配的文件不能进入运行层。
const UNanamiLocalVfxComponent::FClip* UNanamiLocalVfxComponent::GetClip(const FString& ClipName)
{
    if (const TUniquePtr<FClip>* Existing = Clips.Find(ClipName))
    {
        return Existing->Get();
    }
    TUniquePtr<FClip> Clip = MakeUnique<FClip>();
    if (!FFileHelper::LoadFileToArray(Clip->Bytes, *DataPath(ClipName + TEXT(".pgrfx"))) || Clip->Bytes.Num() < 12)
    {
        return nullptr;
    }
    FMemoryReader Reader(Clip->Bytes);
    int32 Magic = 0, NumLayers = 0, NumFrames = 0;
    Reader << Magic << NumLayers << NumFrames;
    if (Magic != 0x31584650 || NumLayers < 1 || NumLayers > 64 || NumFrames < 1 || NumFrames > 240)
    {
        return nullptr;
    }
    for (int32 I = 0; I < NumLayers; ++I)
    {
        int32 Length = 0;
        Reader << Length;
        if (Length < 1 || Length > 99 || Reader.Tell() + Length > Reader.TotalSize())
        {
            return nullptr;
        }
        ANSICHAR Text[100] = {};
        Reader.Serialize(Text, Length);
        Clip->Layers.Add(ANSI_TO_TCHAR(Text));
    }
    for (int32 F = 0; F < NumFrames; ++F)
    {
        TArray<int32>& Offsets = Clip->Offsets.AddDefaulted_GetRef();
        for (int32 L = 0; L < NumLayers; ++L)
        {
            int32 Size = 0;
            Reader << Size;
            if (Reader.IsError() || Size < 0 || (Size > 0 && Size < 12) || Reader.Tell() + Size > Reader.TotalSize())
            {
                return nullptr;
            }
            Offsets.Add(Size ? static_cast<int32>(Reader.Tell()) : INDEX_NONE);
            Reader.Seek(Reader.Tell() + Size);
        }
    }
    if (Reader.Tell() != Reader.TotalSize())
    {
        return nullptr;
    }
    return Clips.Add(ClipName, MoveTemp(Clip)).Get();
}

// 将参考坐标解释为角色朝向下的世界残留、地面或武器骨局部空间。
FTransform UNanamiLocalVfxComponent::GetOrigin(const FCue& Cue) const
{
    const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    const USkeletalMeshComponent* Mesh = CharacterMesh.Get();
    if (!OwnerCharacter || !Mesh)
    {
        return FTransform::Identity;
    }
    FTransform Origin;
    FName AttachBone = NAME_None;
    if (Cue.Anchor == TEXT("Weapon")) AttachBone = WeaponAnchor;
    else if (Cue.Anchor == TEXT("Prop1")) AttachBone = TEXT("Bip001Prop1");
    else if (Cue.Anchor == TEXT("Sword03Blade")) AttachBone = TEXT("Blade_WeaponBone1");
    else if (Cue.Anchor == TEXT("Sword03Shield")) AttachBone = TEXT("Aegis_WeaponCenter");
    else if (Cue.Anchor == TEXT("Weapon2")) AttachBone = TEXT("Aegis_WeaponBone2");
    else if (Cue.Anchor == TEXT("LeftFoot")) AttachBone = TEXT("Bip001LFoot");
    else if (Cue.Anchor == TEXT("RightFoot")) AttachBone = TEXT("Bip001RFoot");
    if (!AttachBone.IsNone() && !Mesh->DoesSocketExist(AttachBone))
    {
        UE_LOG(LogNanamiLocalVfx, Warning, TEXT("PGR_VFX_MISSING_ANCHOR %s %s"), *Cue.Clip, *AttachBone.ToString());
    }
    if (!AttachBone.IsNone() && Mesh->DoesSocketExist(AttachBone))
    {
        Origin = Mesh->GetSocketTransform(AttachBone);
    }
    else
    {
        // 四段的源网格沿反方向旋转，用实机斧刃上挑向量校正，保持相对角色朝向。
        const double SwingYaw = (Cue.Clip == TEXT("Attack64") || Cue.Clip == TEXT("Attack64_Ex")) ? -145.0 : 0.0;
        const bool bDodge = Cue.Anchor == TEXT("DodgeGround");
        double Yaw = OwnerCharacter->GetActorRotation().Yaw + SwingYaw;
        if (bDodge && Cue.Action == TEXT("DodgeTravel"))
        {
            if (const UCombatComponent* Combat = OwnerCharacter->FindComponentByClass<UCombatComponent>())
            {
                const FVector Direction = Combat->GetActiveDodgeDirection();
                if (!Direction.IsNearlyZero())
                {
                    Yaw = Direction.Rotation().Yaw;
                }
            }
        }
        const FRotator Facing(0, Yaw, 0);
        FVector P = Mesh->GetComponentLocation();
        if (Cue.Anchor == TEXT("Ground") || bDodge)
        {
            const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
            FVector Foot = Capsule->GetComponentLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
            if (Cue.Clip == TEXT("Attack62_Impact") || Cue.Clip.EndsWith(TEXT("_ExImpact")))
            {
                Foot += Facing.RotateVector(FVector(Cue.Offset.X, Cue.Offset.Y, 0));
            }
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(LocalVfxGround), false, OwnerCharacter);
            P = GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 25), Foot - FVector(0, 0, 320),
                ECC_WorldStatic, Query) ? Hit.ImpactPoint : Foot;
            P.Z += 2;
        }
        // 盾斧回剑盾时动画带起身体，但网格组件仍在地面；只补偿骨骼腾空的高度。
        if (Cue.Clip == TEXT("Attack32") && Mesh->GetBoneIndex(PelvisAnchor) != INDEX_NONE)
        {
            const double PelvisZ = Mesh->GetSocketTransform(PelvisAnchor, RTS_Component).GetLocation().Z;
            P.Z += FMath::Clamp(PelvisZ - YellowReturnStartPelvisZ, 0.0, 180.0);
        }
        Origin = FTransform(Facing, P);
    }
    const FVector LocalOffset = Cue.Clip == TEXT("Attack62_Impact") || Cue.Clip.EndsWith(TEXT("_ExImpact"))
        ? FVector(0, 0, Cue.Offset.Z) : Cue.Offset;
    if (Cue.Anchor == TEXT("Sword03Blade") && Mesh->DoesSocketExist(AttachBone) &&
        Mesh->DoesSocketExist(TEXT("Blade_WeaponCenter")))
    {
        const FVector Hilt = Origin.GetLocation();
        const FVector End = Mesh->GetSocketLocation(TEXT("Blade_WeaponCenter"));
        const FVector Axis = (End - Hilt).GetSafeNormal();
        if (!Axis.IsNearlyZero())
        {
            const FVector Up = Origin.GetRotation().GetAxisZ();
            Origin.SetRotation(FRotationMatrix::MakeFromXZ(Axis, Up).ToQuat());
            Origin.SetLocation(Hilt + Axis * 47.f);
        }
    }
    if (Cue.Anchor == TEXT("Sword03Shield") && Mesh->DoesSocketExist(AttachBone))
    {
        // Use the face axis of the in-game shield socket;
        // the halo clip is authored in XY (local normal Z).
        const FQuat ShieldFrame = Origin.GetRotation();
        Origin.SetRotation(FRotationMatrix::MakeFromZX(ShieldFrame.GetAxisZ(), ShieldFrame.GetAxisX()).ToQuat());
    }
    Origin.AddToTranslation(Origin.GetRotation().RotateVector(LocalOffset));
    Origin.SetScale3D(FVector(Cue.Scale));
    return Origin;
}

// 启动一份独立播放实例；新连段不会移动已脱手的残留特效。
void UNanamiLocalVfxComponent::StartCue(const FCue& Cue, int32 MontageId, double Overshoot)
{
    if (Cue.Anchor == TEXT("Weapon") && (!CharacterMesh.IsValid() || !CharacterMesh->DoesSocketExist(WeaponAnchor)))
    {
        UE_LOG(LogNanamiLocalVfx, Warning, TEXT("PGR_VFX_SKIP %s: missing Bip001Prop2"), *Cue.Clip);
        return;
    }
    const FClip* Clip = GetClip(Cue.Clip);
    if (!Clip)
    {
        UE_LOG(LogNanamiLocalVfx, Warning, TEXT("PGR_VFX_SKIP %s: data unavailable"), *Cue.Clip);
        return;
    }
    FPlayback Effect;
    Effect.Cue = Cue;
    Effect.Clip = Clip;
    Effect.Age = Overshoot;
    Effect.MontageId = MontageId;
    Effect.Origin = GetOrigin(Cue);
    Effect.Planted.Init(false, Clip->Layers.Num());
    int32 Loaded = 0;
    for (int32 I = 0; I < Clip->Layers.Num(); ++I)
    {
        const FString& Layer = Clip->Layers[I];
        const FString Asset = FString::Printf(TEXT("MI_%s"), *Layer);
        const FString Scope = Cue.Clip == TEXT("Attack03_SwordCharge")
            ? (Layer.StartsWith(TEXT("A102")) ? TEXT("Attack04_Charge") :
                Layer.StartsWith(TEXT("A101")) ? TEXT("Attack03_Blade") : TEXT("Attack03_Air")) : Cue.Clip;
        const FString Path = FString::Printf(TEXT("/Game/LocalOnly/NanamiVfx/%s/Native/%s.%s"), *Scope, *Asset, *Asset);
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
        if (!Material)
        {
            UE_LOG(LogNanamiLocalVfx, Warning, TEXT("PGR_VFX_MISSING_MATERIAL %s"), *Path);
            Effect.Layers.Add(nullptr);
            continue;
        }
        auto* Part = NewObject<UProceduralMeshComponent>(GetOwner());
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        const bool bAxeEdge = Cue.Clip.EndsWith(TEXT("_Ex")) &&
            (Layer.EndsWith(TEXT("_Saw")) || Layer.EndsWith(TEXT("_SawA")));
        Part->SetTranslucentSortPriority(bAxeEdge ? 70 + I : I);
        if (Layer.EndsWith(TEXT("_Mask")) && !Layer.StartsWith(TEXT("L")) &&
            (Cue.Clip.EndsWith(TEXT("_ExImpact")) || Cue.Clip.StartsWith(TEXT("Attack43_")) || Cue.Clip.StartsWith(TEXT("Attack44_")) ||
             Cue.Clip.StartsWith(TEXT("Attack99_")) || Cue.Clip.StartsWith(TEXT("Attack48_"))))
        {
            Part->SetRenderInMainPass(false);
            Part->SetRenderInDepthPass(false);
            Part->SetRenderCustomDepth(true);
            Part->SetCustomDepthStencilValue(Cue.Clip == TEXT("Attack64_ExImpact") ? 6 : 1);
        }
        Part->SetWorldTransform(Effect.Origin);
        Part->SetMaterial(0, UMaterialInstanceDynamic::Create(Material, Part));
        Part->SetVisibility(false);
        GetOwner()->AddInstanceComponent(Part);
        Part->RegisterComponent();
        Effect.Layers.Add(Part);
        ++Loaded;
    }
    UE_LOG(LogNanamiLocalVfx, Display, TEXT("PGR_VFX_CUE %s %s t=%.3f anchor=%s pos=%s layers=%d/%d"),
        *Cue.Action.ToString(), *Cue.Clip, Cue.Seconds, *Cue.Anchor, *Effect.Origin.GetLocation().ToCompactString(), Loaded, Clip->Layers.Num());
    Active.Add(MoveTemp(Effect));
    // 仅在截图诊断中，将回归用例临时接管的 Pawn 设为镜头目标。
    if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
    {
        FString ShotFolder;
        if (FParse::Value(FCommandLine::Get(), TEXT("PGRVfxShots="), ShotFolder) &&
            Player->GetPawn() == GetOwner() && Player->GetViewTarget() != GetOwner())
        {
            Player->SetViewTarget(GetOwner());
        }
    }
}

// 把快照的原始坐标、UV 及顶点色还原为 UE 三维可渲染网格。
void UNanamiLocalVfxComponent::ApplyFrame(FPlayback& Effect, int32 Frame) const
{
    const FClip& Clip = *Effect.Clip;
    FMemoryReader Reader(Clip.Bytes);
    int32 Visible = 0;
    for (int32 L = 0; L < Clip.Layers.Num(); ++L)
    {
        UProceduralMeshComponent* Part = Effect.Layers[L].Get();
        if (!Part)
        {
            continue;
        }
        const FString& Layer = Clip.Layers[L];
        if (Effect.Cue.Clip == TEXT("Attack64_ExImpact"))
        {
            FString OmitLayers;
            if (FParse::Value(FCommandLine::Get(),TEXT("AxeOmitLayers="),OmitLayers) && OmitLayers.Contains(Layer))
            {
                Part->SetVisibility(false);
                continue;
            }
        }
        const int32 AxeLayer = Layer.StartsWith(TEXT("A6")) ? FCString::Atoi(*Layer.Mid(5, 2)) : INDEX_NONE;
        const bool bAxeFollowup = (Effect.Cue.Clip == TEXT("Attack62") && AxeLayer >= 8) ||
            (Effect.Cue.Clip == TEXT("Attack62_Ex") && AxeLayer >= 13);
        const bool bSecondImpact = (Effect.Cue.Clip == TEXT("Attack62_Impact") && AxeLayer >= 26) ||
            (Effect.Cue.Clip == TEXT("Attack62_ExImpact") && AxeLayer >= 40);
        // 同一招的两刀以各自的接触帧为准，不用同一个时移。
        const int32 LayerFrame = Frame + (bAxeFollowup ? 3 : bSecondImpact ? 4 : 0);
        if (!Clip.Offsets.IsValidIndex(LayerFrame))
        {
            Part->SetVisibility(false);
            continue;
        }
        const bool bWorld = Effect.Cue.Anchor == TEXT("World") || Effect.Cue.Clip == TEXT("Attack31_Weapon") ||
            Effect.Cue.Clip == TEXT("Attack62_Impact") || Effect.Cue.Clip.EndsWith(TEXT("_ExImpact"));
        if (Effect.Cue.Anchor == TEXT("DodgeGround") && Effect.MontageId == PlannedMontageId &&
            (Layer.EndsWith(TEXT("_Chong")) || Layer.EndsWith(TEXT("_Thunder02"))))
        {
            Part->SetWorldTransform(GetOrigin(Effect.Cue));
        }
        if (Effect.Cue.bActionBound && !bWorld && Layer.StartsWith(TEXT("A1")) && Effect.MontageId == PlannedMontageId)
        {
            Part->SetWorldTransform(GetOrigin(Effect.Cue));
        }
        if (bWorld && !Effect.Planted[L])
        {
            const int32 PlantFrame = WorldPlantFrame(Effect.Cue.Clip, Layer);
            const bool bContinueFighterAnchor = !Layer.StartsWith(TEXT("A6")) || Effect.MontageId == PlannedMontageId;
            if (bContinueFighterAnchor)
            {
                Part->SetWorldTransform(GetOrigin(Effect.Cue));
            }
            Effect.Planted[L] = LayerFrame >= PlantFrame;
            if (Effect.Planted[L] &&
                (Layer.EndsWith(TEXT("_An")) || Layer.EndsWith(TEXT("_DilieAn"))) &&
                Layer.StartsWith(TEXT("A6")))
            {
                const FVector Center = Part->GetComponentLocation();
                const FVector Blade = CharacterMesh->GetSocketLocation(WeaponAnchor);
                UE_LOG(LogNanamiLocalVfx, Display, TEXT("PGR_AXE_PLANT %s %s f=%d base=%s weapon=%s ground=%d yaw=%.1f"),
                    *Effect.Cue.Clip, *Layer, LayerFrame, *Center.ToCompactString(), *Blade.ToCompactString(),
                    Effect.Cue.Anchor == TEXT("Ground"), Part->GetComponentRotation().Yaw);
            }
        }
        int32 Sample = LayerFrame;
        if (Effect.Cue.Clip == TEXT("Attack22") && (Layer == TEXT("L08_Lei01") || Layer == TEXT("L09_Lei02")))
        {
            for (int32 F = Frame; F > Effect.LastFrame && F > Frame - 3; --F)
            {
                if (IsBoltPulse(Layer, F))
                {
                    Sample = F;
                    break;
                }
            }
        }
        const int32 Offset = Clip.Offsets[Sample][L];
        if (Offset == INDEX_NONE)
        {
            Part->SetVisibility(false);
            continue;
        }
        Reader.Seek(Offset);
        float CustomUV = 0;
        int32 Vertices = 0, Indices = 0;
        Reader << CustomUV << Vertices << Indices;
        if (Vertices <= 0 || Vertices > 500000 || Indices <= 0 || Indices > 1000000 || Indices % 3 ||
            Reader.Tell() + int64(Vertices) * 24 + int64(Indices) * 4 > Reader.TotalSize())
        {
            continue;
        }
        TArray<FVector> Positions, Normals;
        TArray<FVector2D> UV0, UV1;
        TArray<FLinearColor> Colors;
        TArray<int32> Triangles;
        Positions.Reserve(Vertices);
        UV0.Reserve(Vertices);
        UV1.Reserve(Vertices);
        Colors.Reserve(Vertices);
        for (int32 J = 0; J < Vertices; ++J)
        {
            float X, Y, Z, U, V;
            uint8 R, G, B, A;
            Reader << X << Y << Z << U << V << R << G << B << A;
            if (Effect.Cue.Clip == TEXT("Attack01") && Layer == TEXT("Qiliu"))
            {
                Y += .07f;
                Z -= 1.75f;
            }
            Positions.Emplace(Z * 100, X * 100, Y * 100);
            UV0.Emplace(U, V);
            UV1.Emplace(A / 255.f, CustomUV);
            Colors.Emplace(R / 255.f, G / 255.f, B / 255.f, A / 255.f);
        }
        Triangles.SetNumUninitialized(Indices);
        bool bValid = true;
        for (int32& J : Triangles)
        {
            Reader << J;
            bValid &= Positions.IsValidIndex(J);
        }
        if (!bValid)
        {
            continue;
        }
        if (Effect.Cue.Clip == TEXT("Attack11_Smoke") && Layer == TEXT("L05_StoneLizi"))
        {
            Normals.Init(FVector::ZeroVector, Vertices);
            for (int32 I = 0; I < Indices; I += 3)
            {
                const int32 A = Triangles[I], B = Triangles[I + 1], C = Triangles[I + 2];
                const FVector Normal = FVector::CrossProduct(Positions[B] - Positions[A], Positions[C] - Positions[A]);
                Normals[A] += Normal;
                Normals[B] += Normal;
                Normals[C] += Normal;
            }
            for (FVector& N : Normals)
            {
                N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
            }
        }
        if (auto* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
        {
            SetMaterialFrame(*Material, Effect.Cue.Clip, Layer, Sample);
        }
        Part->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, UV0, UV1, {}, {}, Colors, {}, false, false);
        Part->SetVisibility(true);
        ++Visible;
    }
    if (Visible > 0)
    {
        static const TMap<FString, int32> ShotFrames = {
            {TEXT("Attack01"), 10}, {TEXT("Attack02"), 29}, {TEXT("Attack03"), 20}, {TEXT("Attack04"), 53},
            {TEXT("Attack11"), 19}, {TEXT("Attack11_Smoke"), 12}, {TEXT("Attack12"), 7}, {TEXT("Attack12_Impact"), 26},
            {TEXT("Attack21"), 7}, {TEXT("Attack22"), 31}, {TEXT("Attack22_Weapon"), 22},
            {TEXT("Attack31"), 36}, {TEXT("Attack31_Weapon"), 20}, {TEXT("Attack32"), 4}, {TEXT("Attack32_Ci"), 10},
            {TEXT("Attack61"), 6}, {TEXT("Attack62"), 8}, {TEXT("Attack62_Impact"), 10},
            {TEXT("Attack63"), 28}, {TEXT("Attack64"), 5},
            {TEXT("Attack61_Ex"),6},{TEXT("Attack62_Ex"),6},{TEXT("Attack62_ExImpact"),10},
            {TEXT("Attack63_Ex"),28},{TEXT("Attack64_Ex"),5},{TEXT("Attack64_ExImpact"),10},
            {TEXT("AttackDodgeMove1"),4},{TEXT("AttackDodgeMove2"),4}};
        const int32* Target = ShotFrames.Find(Effect.Cue.Clip);
        if (Target && Frame >= *Target && Effect.LastFrame < *Target)
        {
            UE_LOG(LogNanamiLocalVfx, Display, TEXT("PGR_VFX_VISIBLE %s f=%d layers=%d"), *Effect.Cue.Clip, Frame, Visible);
            FString ShotFolder;
            if (FParse::Value(FCommandLine::Get(), TEXT("PGRVfxShots="), ShotFolder) && !ShotFolder.IsEmpty())
            {
                IFileManager::Get().MakeDirectory(*ShotFolder, true);
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, Effect.Cue.Clip + TEXT(".png")), false, false);
            }
        }
    }
    Effect.LastFrame = Frame;
}

// 销毁独立渲染件，不留下已被打断的跟武器层。
void UNanamiLocalVfxComponent::DestroyPlayback(FPlayback& Effect)
{
    for (const auto& Layer : Effect.Layers)
    {
        if (auto* Part = Layer.Get())
        {
            Part->DestroyComponent();
        }
    }
}

// 动作中断只撤销未触发事件和跟随层，落地残影按自己的时间自然退场。
void UNanamiLocalVfxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UAnimInstance* Anim = PlannedAnim.Get();
    const FAnimMontageInstance* Current = Anim ? Anim->GetMontageInstanceForID(PlannedMontageId) : nullptr;
    const bool bLive = Current && Current->IsActive();
    if (SwordTrailMontage != INDEX_NONE)
    {
        if (bLive && Current->GetInstanceID() == SwordTrailMontage)
        {
            TickSwordTrail(Current->GetPosition() - PlannedSectionStart);
        }
        else
        {
            ResetSwordTrail();
        }
    }
    for (int32 I = Active.Num() - 1; I >= 0; --I)
    {
        FPlayback& Fx = Active[I];
        Fx.Age += DeltaTime;
        const int32 Frame = FMath::FloorToInt(Fx.Age * 60.0 + 1e-5);
        if (!Fx.Clip->Offsets.IsValidIndex(Frame) || (Fx.Cue.bActionBound && (!bLive || Fx.MontageId != PlannedMontageId)))
        {
            DestroyPlayback(Fx);
            Active.RemoveAtSwap(I);
            continue;
        }
        if ((Fx.Cue.Anchor == TEXT("Weapon") && Fx.Cue.Clip != TEXT("Attack31_Weapon")) || Fx.Cue.Anchor == TEXT("Root") || Fx.Cue.Anchor == TEXT("Sword03Shield") || Fx.Cue.Anchor == TEXT("Sword03Blade"))
        {
            const FTransform T = GetOrigin(Fx.Cue);
            for (const auto& Part : Fx.Layers)
            {
                if (Part.IsValid())
                {
                    Part->SetWorldTransform(T);
                }
            }
        }
        if (Fx.Cue.Clip == TEXT("Attack03_SwordCharge") && Frame != Fx.LastFrame && Frame % 5 == 0)
        {
            const FTransform T = GetOrigin(Fx.Cue);
            UE_LOG(LogNanamiLocalVfx, Display, TEXT("PGR_SWORD03_BLADE_CHARGE f=%d center=%s axis=%s"),Frame,
                *T.GetLocation().ToCompactString(), *T.GetRotation().GetAxisX().ToCompactString());
        }
        if (Fx.Cue.Clip == TEXT("Attack03_ShieldGlow") && Frame != Fx.LastFrame && Frame % 6 == 0 && CharacterMesh.IsValid())
        {
            const FTransform Halo = GetOrigin(Fx.Cue);
            const FTransform Shield = CharacterMesh->GetSocketTransform(TEXT("Aegis_WeaponCenter"));
            const FVector C = Halo.GetLocation();
            const FVector P = CharacterMesh->GetSocketLocation(TEXT("Aegis_WeaponBone2"));
            const FVector B = CharacterMesh->GetSocketLocation(WeaponAnchor);
            UE_LOG(LogNanamiLocalVfx, Display, TEXT("PGR_SWORD03_SHIELD_POSE f=%d center=%s shield=%s grip=%s distance=%.1f faceAlignment=%.4f sx=%s sy=%s sz=%s"), Frame,
                *C.ToCompactString(), *P.ToCompactString(), *B.ToCompactString(), FVector::Dist(C, B),
                FVector::DotProduct(Halo.GetRotation().GetAxisZ(), Shield.GetRotation().GetAxisZ()),
                *Shield.GetRotation().GetAxisX().ToCompactString(), *Shield.GetRotation().GetAxisY().ToCompactString(),
                *Shield.GetRotation().GetAxisZ().ToCompactString());
        }
        if (Frame != Fx.LastFrame)
        {
            ApplyFrame(Fx, Frame);
        }
    }
    if (!bLive)
    {
        Pending.Reset();
    }
    else
    {
        const float Seconds = Current->GetPosition() - PlannedSectionStart;
        if (!bYellowHoldExitSeen)
        {
            const auto* Motion = GetOwner()->FindComponentByClass<UCombatMotionComponent>();
            if (Motion && !Motion->GetExitSection().IsNone() &&
                Motion->GetCurrentActionSection() == Motion->GetExitSection())
            {
                bYellowHoldExitSeen = true;
                Pending.Reset();
                for (const FCue& Cue : Cues)
                {
                    if (Cue.Action == TEXT("YellowHoldExit"))
                    {
                        StartCue(Cue, PlannedMontageId, 0.f);
                    }
                }
            }
        }
        for (int32 I = 0; I < Pending.Num();)
        {
            const FCue& Cue = Cues[Pending[I]];
            if (Seconds < Cue.Seconds)
            {
                ++I;
                continue;
            }
            StartCue(Cue, PlannedMontageId, FMath::Clamp(Seconds - Cue.Seconds, 0.f, 1.f / 30.f));
            if (!Active.IsEmpty() && Active.Last().LastFrame == INDEX_NONE)
            {
                ApplyFrame(Active.Last(), FMath::FloorToInt(Active.Last().Age * 60.0));
            }
            Pending.RemoveAt(I);
        }
    }
    if (Active.IsEmpty() && Pending.IsEmpty() && SwordTrailMontage == INDEX_NONE)
    {
        SetComponentTickEnabled(false);
    }
}



