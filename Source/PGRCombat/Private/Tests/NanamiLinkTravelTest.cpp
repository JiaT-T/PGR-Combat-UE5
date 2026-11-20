#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Motion/CombatMotionComponent.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace
{
	struct FLinkTravelSample
	{
		FVector Capsule;
		FVector Root;
		FVector Pelvis;

		// 从游戏世界同步取得碰撞胶囊和装配骨架上两个可视参考点的位置。
		static FLinkTravelSample Read(const ACharacter& Character)
		{
			const USkeletalMeshComponent* Mesh = Character.GetMesh();
			return {Character.GetActorLocation(), Mesh->GetSocketLocation(TEXT("Root")), Mesh->GetSocketLocation(TEXT("Bip001Pelvis"))};
		}
	};

	struct FLinkTravelTrace
	{
		FLinkTravelSample Start{};
		FLinkTravelSample Cut{};
		FVector Direction = FVector::ZeroVector;
		FVector LastAttackCapsule = FVector::ZeroVector;
		double CutTime = 0.0;
		float CutMontageTime = 0.0f;
		double IdleSince = -1.0;
		double WorstRoot = 0.0;
		double WorstPelvis = 0.0;
		double WorstCapsule = 0.0;
		bool bStarted = false;
		bool bLinked = false;
		bool bMeasured = false;
		bool bSampledFirstFrame = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRNanamiLinkTravelTest, "PGR.Combat.BasicAttack.NanamiLinkTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 分别在第一段刚开窗和持盾稍后续招，采样根运动与可视骨骼的世界水平轨迹。
bool FPGRNanamiLinkTravelTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	TWeakObjectPtr<ACharacter> Fighter = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
	TWeakObjectPtr<UCombatComponent> Combat = Fighter.IsValid() ? Fighter->FindComponentByClass<UCombatComponent>() : nullptr;
	TWeakObjectPtr<UCombatMotionComponent> Motion = Fighter.IsValid() ? Fighter->FindComponentByClass<UCombatMotionComponent>() : nullptr;
	if (!TestTrue(TEXT("Nanami training map and possessed fighter required"), Fighter.IsValid() && Combat.IsValid() && Motion.IsValid()
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/Training/Nanami/L_NanamiTraining")))
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = Fighter->GetMesh();
	if (!TestTrue(TEXT("Equipped mesh has visual Root and pelvis"), Mesh && Mesh->GetBoneIndex(TEXT("Root")) != INDEX_NONE
		&& Mesh->GetBoneIndex(TEXT("Bip001Pelvis")) != INDEX_NONE))
	{
		return false;
	}

	for (const bool bAtWindowStart : {true, false})
	{
		const TSharedRef<FLinkTravelTrace> Trace = MakeShared<FLinkTravelTrace>();
		// 每帧先读世界坐标，再按首段 Montage 的本地时间执行真实续招。
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand([this, Fighter, Combat, Motion, Trace, bAtWindowStart]()
		{
			if (!Fighter.IsValid() || !Combat.IsValid() || !Motion.IsValid())
			{
				AddError(TEXT("Fighter destroyed during world-space trace"));
				return true;
			}
			const FLinkTravelSample Pose = FLinkTravelSample::Read(*Fighter.Get());
			if (!Trace->bStarted)
			{
				Trace->Start = Pose;
				Trace->bStarted = true;
				return !TestTrue(TEXT("Start Attack01 for travel trace"),
					Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed) == ECombatInputResult::Started);
			}
			if (!Trace->bLinked)
			{
				const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
				const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
				const float Position = Montage ? Anim->Montage_GetPosition(Montage) : 0.0f;
				// 首轮采用动作表驱动的实际开窗，第二轮在已开窗的 1 秒处触发。
				if (!Motion->IsInputBufferOpen() || (!bAtWindowStart && Position < 1.0f))
				{
					return false;
				}
				Trace->Cut = Pose;
				Trace->CutMontageTime = Position;
				Trace->CutTime = Fighter->GetWorld()->GetTimeSeconds();
				Trace->bLinked = true;
				const FVector Travel = Pose.Root - Trace->Start.Root;
				Trace->Direction = Travel.GetSafeNormal2D();
				AddInfo(FString::Printf(TEXT("Link at %.3f s: capsule %s -> %s, Root %s -> %s, pelvis %s"),
					Position, *Trace->Start.Capsule.ToString(), *Pose.Capsule.ToString(), *Trace->Start.Root.ToString(),
					*Pose.Root.ToString(), *Pose.Pelvis.ToString()));
				// 两个取样姿势在原片均已突进约 2 m，容许混入差异并防止模型比例被重复应用。
				const double FirstCapsule = FVector::DotProduct(Pose.Capsule - Trace->Start.Capsule, Trace->Direction);
				TestTrue(TEXT("Attack01 advances capsule and visible Root on the authored metre scale"),
					FMath::IsWithinInclusive(Travel.Size2D(), 120.0, 290.0) && FMath::IsWithinInclusive(FirstCapsule, 120.0, 290.0));
				return !TestTrue(TEXT("Attack02 starts at the requested cut"),
					Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed) == ECombatInputResult::Started
					&& Combat->GetActiveActionId() == TEXT("Attack02"));
			}
			if (Trace->bMeasured)
			{
				if (Combat->GetActiveActionId() == TEXT("Attack02"))
				{
					Trace->LastAttackCapsule = Pose.Capsule;
					return false;
				}
				if (!TestTrue(TEXT("Attack02 finished naturally into idle"), Combat->GetActiveActionId().IsNone()))
				{
					return true;
				}
				const double Now = Fighter->GetWorld()->GetTimeSeconds();
				if (Trace->IdleSince < 0.0)
				{
					Trace->IdleSince = Now;
				}
				if (Now - Trace->IdleSince < 0.20)
				{
					return false;
				}
				const double Shift = FVector::Dist2D(Trace->LastAttackCapsule, Pose.Capsule);
				const double Forward = FVector::DotProduct(Pose.Capsule - Trace->Cut.Capsule, Trace->Direction);
				AddInfo(FString::Printf(TEXT("Link %.2f s: last Attack02 capsule %s, settled idle %s, shift %.2f cm, retained %.1f cm past link"),
					Trace->CutMontageTime, *Trace->LastAttackCapsule.ToString(), *Pose.Capsule.ToString(), Shift, Forward));
				TestTrue(TEXT("Idle keeps the Attack02 landing point"), Shift <= 5.0 && Forward > 180.0);
				return true;
			}
			const double Elapsed = Fighter->GetWorld()->GetTimeSeconds() - Trace->CutTime;
			if (!Trace->bSampledFirstFrame)
			{
				AddInfo(FString::Printf(TEXT("First pose after %.2f s cut (+%.3f s): capsule %s, Root %s, pelvis %s"),
					Trace->CutMontageTime, Elapsed, *Pose.Capsule.ToString(), *Pose.Root.ToString(), *Pose.Pelvis.ToString()));
				Trace->bSampledFirstFrame = true;
			}
			const double Root = FVector::DotProduct(Pose.Root - Trace->Cut.Root, Trace->Direction);
			const double Pelvis = FVector::DotProduct(Pose.Pelvis - Trace->Cut.Pelvis, Trace->Direction);
			const double Capsule = FVector::DotProduct(Pose.Capsule - Trace->Cut.Capsule, Trace->Direction);
			Trace->WorstRoot = FMath::Min(Trace->WorstRoot, Root);
			Trace->WorstPelvis = FMath::Min(Trace->WorstPelvis, Pelvis);
			Trace->WorstCapsule = FMath::Min(Trace->WorstCapsule, Capsule);
			if (Elapsed >= 0.60)
			{
				Trace->bMeasured = true;
				Trace->LastAttackCapsule = Pose.Capsule;
				AddInfo(FString::Printf(TEXT("Link %.2f s +%.3f s: capsule %s, Root %s, pelvis %s; forward cm %.1f/%.1f/%.1f; "
					"minimum cm %.1f/%.1f/%.1f (capsule/Root/pelvis)"), Trace->CutMontageTime, Elapsed, *Pose.Capsule.ToString(), *Pose.Root.ToString(),
					*Pose.Pelvis.ToString(), Capsule, Root, Pelvis, Trace->WorstCapsule, Trace->WorstRoot, Trace->WorstPelvis));
				// 原第二段起手自带约 4–6 cm 的短后撤，明显超过这段预备动作视为回跳。
				TestTrue(TEXT("No substantial pullback behind the link position"),
					Trace->WorstCapsule >= -12 && Trace->WorstRoot >= -12 && Trace->WorstPelvis >= -12);
				// 第二段 0.6 秒时原片约前行 3.25 m，胶囊、根骨和骨盆均应在同一数量级。
				TestTrue(TEXT("Attack02 carries both collision and visible body forward on the authored metre scale"),
					FMath::IsWithinInclusive(Root, 180.0, 420.0) && FMath::IsWithinInclusive(Pelvis, 180.0, 420.0)
					&& FMath::IsWithinInclusive(Capsule, 180.0, 420.0));
			}
			return false;
		},
			// 报告未达到切点、采样时间或自然收招的场景。
			[this, bAtWindowStart]() { AddError(bAtWindowStart ? TEXT("Travel trace timed out at opening") : TEXT("Travel trace timed out at late cut")); return true; }, 12.0f));
	}
	return true;
}

#endif
