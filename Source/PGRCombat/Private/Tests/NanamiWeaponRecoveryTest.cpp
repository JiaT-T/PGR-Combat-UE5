#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Combat/CombatComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Motion/CombatMotionComponent.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace
{
	struct FNanamiWeaponPose
	{
		FVector ShieldFromLeftHand;
		FVector SwordFromRightHand;
		double ShieldSocketError;
		double ShieldHeight;
		double SwordScale;

		// 在角色实际姿势上取左右手局部朝向下的厘米偏移、剑变形和盾距地面高度。
		static FNanamiWeaponPose Read(const ACharacter& Fighter)
		{
			const USkeletalMeshComponent& Mesh = *Fighter.GetMesh();
			const FTransform Left = Mesh.GetSocketTransform(TEXT("Bip001LHand"));
			const FTransform Right = Mesh.GetSocketTransform(TEXT("Bip001RHand"));
			const FVector Shield = Mesh.GetSocketLocation(TEXT("Aegis_WeaponBone2"));
			const FVector Sword = Mesh.GetSocketLocation(TEXT("Blade_WeaponBone1"));
			const double Ground = Fighter.GetCharacterMovement()->CurrentFloor.HitResult.ImpactPoint.Z;
			return {Left.GetRotation().UnrotateVector(Shield - Left.GetLocation()),
				Right.GetRotation().UnrotateVector(Sword - Right.GetLocation()),
				FVector::Dist(Shield, Mesh.GetSocketLocation(TEXT("WeaponCase2"))), Shield.Z - Ground,
				Mesh.GetSocketTransform(TEXT("Blade_SwordBone06"), RTS_ParentBoneSpace).GetScale3D().X};
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRNanamiWeaponRecoveryTest, "PGR.Combat.BasicAttack.NanamiWeaponRecovery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 在相同落脚点分别走完四段和中途打断，验证抛盾演出与实际待机双手姿势。
bool FPGRNanamiWeaponRecoveryTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	TWeakObjectPtr<ACharacter> Fighter = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
	TWeakObjectPtr<UCombatComponent> Combat = Fighter.IsValid() ? Fighter->FindComponentByClass<UCombatComponent>() : nullptr;
	TWeakObjectPtr<UCombatMotionComponent> Motion = Fighter.IsValid() ? Fighter->FindComponentByClass<UCombatMotionComponent>() : nullptr;
	if (!TestTrue(TEXT("Weapon probe needs the possessed Nanami training pawn"), Combat.IsValid() && Motion.IsValid()
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/Training/Nanami/L_NanamiTraining")))
	{
		return false;
	}
	for (FName Bone : {FName(TEXT("Bip001LHand")), FName(TEXT("Bip001RHand")), FName(TEXT("Aegis_WeaponBone2")),
		FName(TEXT("Blade_WeaponBone1")), FName(TEXT("WeaponCase2")), FName(TEXT("Blade_SwordBone06"))})
	{
		if (!TestTrue(*FString::Printf(TEXT("Equipped skeleton contains %s"), *Bone.ToString()), Fighter->GetMesh()->GetBoneIndex(Bone) != INDEX_NONE))
		{
			return false;
		}
	}
	const FVector InitialLocation = Fighter->GetActorLocation();
	const UPrimitiveComponent* Floor = Fighter->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent();
	if (!TestNotNull(TEXT("Find the training platform below Nanami"), Floor))
	{
		return false;
	}
	const FVector Center(Floor->Bounds.Origin.X, Floor->Bounds.Origin.Y, InitialLocation.Z);
	for (const bool bInterrupt : {false, true})
	{
		// 独立从平台中央开始，防止连续运行其他训练图测试后起点已靠近边缘。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([Fighter, Center]()
		{
			if (Fighter.IsValid())
			{
				Fighter->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}, 0.0f));
		// 读取每场自身的待机基准；提前打断的场景选在剑仍伸展、盾仍在地下的时刻。
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand([this, Fighter, Combat, Motion, bInterrupt, Idle = FNanamiWeaponPose{},
			bStarted = false, bThrow = false, bHidden = false, bReturned = false, bExtended = false,
			FirstHiddenHeight = 0.0, PeakBlendHeight = -1.e6, IdleSince = -1.0]() mutable
		{
			if (!Fighter.IsValid() || !Combat.IsValid() || !Motion.IsValid())
			{
				AddError(TEXT("Lost equipped fighter during weapon probe"));
				return true;
			}
			const FNanamiWeaponPose Pose = FNanamiWeaponPose::Read(*Fighter.Get());
			const FName Action = Combat->GetActiveActionId();
			if (!bStarted)
			{
				if (!TestTrue(TEXT("Capture Stand2 on the training floor"), Action.IsNone() && Fighter->GetCharacterMovement()->CurrentFloor.IsWalkableFloor()))
				{
					return true;
				}
				Idle = Pose;
				bStarted = true;
				return !TestTrue(TEXT("Begin weapon probe from idle"),
					Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed) == ECombatInputResult::Started);
			}
			const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
			const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
			const float T = Montage ? Anim->Montage_GetPosition(Montage) : 0.0f;
			if (Action == TEXT("Attack03") && T >= 0.90f && !bThrow)
			{
				bThrow = true;
				AddInfo(FString::Printf(TEXT("Attack03 throw %.3f s: shield-left hand %.1f cm, shield-socket %.2f cm, height %.1f cm"),
					T, Pose.ShieldFromLeftHand.Size(), Pose.ShieldSocketError, Pose.ShieldHeight));
				TestTrue(TEXT("Shield separates from hand while following its animated mount"), Pose.ShieldFromLeftHand.Size() > 250 && Pose.ShieldSocketError < 3);
			}
			if (Action == TEXT("Attack01") || Action == TEXT("Attack02") || Action == TEXT("Attack03"))
			{
				if (Motion->IsInputBufferOpen())
				{
					if (Action == TEXT("Attack03"))
					{
						FirstHiddenHeight = Pose.ShieldHeight;
						TestTrue(TEXT("Thrown shield is below the training floor at the Attack03 cut"), FirstHiddenHeight < -50);
					}
					TestTrue(TEXT("Continue to the next weapon demonstration"),
						Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed) == ECombatInputResult::Started);
				}
				return false;
			}
			if (Action == TEXT("Attack04"))
			{
				if (!bHidden)
				{
					PeakBlendHeight = FMath::Max(PeakBlendHeight, Pose.ShieldHeight);
					if (T >= 0.18f)
					{
						bHidden = true;
						AddInfo(FString::Printf(TEXT("Shield ground offsets: %.1f cm at Attack03 cut, maximum %.1f cm through first %.3f s of Attack04"),
							FirstHiddenHeight, PeakBlendHeight, T));
						TestTrue(TEXT("Shield stays underground through the link blend"), PeakBlendHeight < -25);
					}
				}
				if (T >= 0.35f && !bExtended)
				{
					bExtended = true;
					AddInfo(FString::Printf(TEXT("Attack04 active sword scale %.3f, Stand2 %.3f"), Pose.SwordScale, Idle.SwordScale));
					TestTrue(TEXT("Attack04 extends the sword beyond its idle size"), Pose.SwordScale > Idle.SwordScale + 0.15);
					if (bInterrupt)
					{
						Combat->InterruptActiveAction();
					}
				}
				if (T >= 3.53f && !bReturned)
				{
					bReturned = true;
					AddInfo(FString::Printf(TEXT("Attack04 %.3f s: shield-left hand %.1f cm, height %.1f cm"), T, Pose.ShieldFromLeftHand.Size(), Pose.ShieldHeight));
					TestTrue(TEXT("Shield returns to the left hand above ground during recovery"), Pose.ShieldFromLeftHand.Size() < 25 && Pose.ShieldHeight > 30);
				}
				return false;
			}
			if (!Action.IsNone())
			{
				AddError(TEXT("Unexpected action during shield probe"));
				return true;
			}
			const double Now = Fighter->GetWorld()->GetTimeSeconds();
			if (IdleSince < 0.0)
			{
				IdleSince = Now;
			}
			if (Now - IdleSince < 0.25)
			{
				return false;
			}
			const double LeftError = FVector::Dist(Idle.ShieldFromLeftHand, Pose.ShieldFromLeftHand);
			const double RightError = FVector::Dist(Idle.SwordFromRightHand, Pose.SwordFromRightHand);
			AddInfo(FString::Printf(TEXT("Stand2 after %s: shield-left hand error %.2f cm, sword-right hand error %.2f cm, sword scale %.3f (baseline %.3f)"),
				bInterrupt ? TEXT("interrupt") : TEXT("completion"), LeftError, RightError, Pose.SwordScale, Idle.SwordScale));
			TestTrue(TEXT("All expected stages of the weapon choreography occurred"), bThrow && bHidden && bExtended && (bInterrupt || bReturned));
			TestTrue(TEXT("Shield and sword resume the Stand2 hand-relative placement and size"), LeftError < 5 && RightError < 5
				&& FMath::Abs(Pose.SwordScale - Idle.SwordScale) < 0.05);
			return true;
		},
			// 标记当前场景中缺失的收招结果。
			[this, bInterrupt]() { AddError(bInterrupt ? TEXT("Interrupted weapon probe timed out") : TEXT("Natural weapon probe timed out")); return true; }, 20.0f));
		// 将角色复位到本场入口，避免下一轮继续累积位移而离开训练平台。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([Combat, Fighter, InitialLocation]()
		{
			if (Combat.IsValid() && Fighter.IsValid())
			{
				Combat->InterruptActiveAction();
				Fighter->SetActorLocation(InitialLocation, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}, 0.0f));
	}
	return true;
}

#endif
