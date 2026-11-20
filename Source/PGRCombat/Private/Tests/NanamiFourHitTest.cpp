#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRNanamiFourHitTest, "PGR.Combat.BasicAttack.NanamiFourHitCadence",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 持续从玩家控制器点按 J，核对剑盾四段的起手顺序、实播时间和世界位移。
bool FPGRNanamiFourHitTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	TWeakObjectPtr<APlayerController> Player = World ? World->GetFirstPlayerController() : nullptr;
	TWeakObjectPtr<ACharacter> Fighter = Player.IsValid() ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
	TWeakObjectPtr<UCombatComponent> Combat = Fighter.IsValid() ? Fighter->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!TestTrue(TEXT("Four-hit cadence requires the equipped Nanami training pawn"),
		Combat.IsValid() && Fighter->GetMesh()->GetBoneIndex(TEXT("Root")) != INDEX_NONE
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/Training/Nanami/L_NanamiTraining")))
	{
		return false;
	}
	const FVector InitialLocation = Fighter->GetActorLocation();
	// 按实际游戏时间逐帧取样，点按节奏独立于动作窗口和当前段的播放位置。
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand([this, Player, Fighter, Combat, Start = -1.0, FirstAction = 0.0,
		StageStart = 0.0, Stage = 0, Presses = 0, bDown = false, Instance = int64(0), LastLocalTime = 0.f,
		StageCapsule = FVector::ZeroVector, StageRoot = FVector::ZeroVector, MaxTravel = 0.0, MaxOffsetError = 0.0]() mutable
	{
		if (!Player.IsValid() || !Fighter.IsValid() || !Combat.IsValid())
		{
			AddError(TEXT("Lost Nanami during four-hit cadence"));
			return true;
		}
		const double Now = Fighter->GetWorld()->GetTimeSeconds();
		if (Start < 0.0)
		{
			Start = Now;
		}
		const double Elapsed = Now - Start;
		const int64 ActiveInstance = Combat->GetActiveInstanceId();
		const FName Action = Combat->GetActiveActionId();
		const FVector Capsule = Fighter->GetActorLocation();
		const FVector Root = Fighter->GetMesh()->GetSocketLocation(TEXT("Root"));
		if (Stage > 0)
		{
			const FVector CapsuleTravel = Capsule - StageCapsule;
			const FVector RootTravel = Root - StageRoot;
			MaxTravel = FMath::Max(MaxTravel, CapsuleTravel.Size2D());
			MaxOffsetError = FMath::Max(MaxOffsetError, FVector::Dist2D(CapsuleTravel, RootTravel));
		}
		if (ActiveInstance != Instance)
		{
			if (Stage > 0)
			{
				AddInfo(FString::Printf(TEXT("Attack%02d exit: played %.3f game s, last Montage sample %.3f s, capsule %s -> %s; peak travel %.1f cm, peak visual offset drift %.1f cm"),
					Stage, Elapsed - StageStart, LastLocalTime, *StageCapsule.ToString(), *Capsule.ToString(), MaxTravel, MaxOffsetError));
				// 第三段本身有前冲与后撤，校验最大位移和可视根骨随动，不限制运动方向。
				TestTrue(TEXT("Capsule moved on the authored metre scale"), MaxTravel > 40.0 && MaxTravel < 1500.0);
				TestTrue(TEXT("Equipped visual Root follows the moving capsule"), MaxOffsetError <= 35.0);
			}
			if (Action.IsNone())
			{
				TestEqual(TEXT("All four attacks played without an idle gap"), Stage, 4);
				AddInfo(FString::Printf(TEXT("Full Montage cycle including recovery: %.3f game s"), Elapsed - FirstAction));
				return true;
			}
			++Stage;
			const FName Expected(*FString::Printf(TEXT("Attack%02d"), Stage));
			if (!TestEqual(TEXT("Next logical action follows the four-hit sequence"), Action, Expected))
			{
				return true;
			}
			if (Stage == 1)
			{
				FirstAction = Elapsed;
			}
			StageStart = Elapsed;
			StageCapsule = Capsule;
			StageRoot = Root;
			MaxTravel = 0.0;
			MaxOffsetError = 0.0;
			LastLocalTime = 0.0f;
			Instance = ActiveInstance;
			AddInfo(FString::Printf(TEXT("%s start: +%.3f s since Attack01, press %d, capsule %s, visual Root %s"),
				*Action.ToString(), Elapsed - FirstAction, Presses, *Capsule.ToString(), *Root.ToString()));
		}
		const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
		const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
		if (Montage)
		{
			const float Position = Anim->Montage_GetPosition(Montage);
			// 这些是按第四段动画姿势识别的参考帧，用于分开报时；实机收刀边界仍待玩家确认。
			if (Stage == 4)
			{
				for (const float Marker : {1.40f, 2.63f, 2.80f})
				{
					if (LastLocalTime < Marker && Position >= Marker)
					{
						AddInfo(FString::Printf(TEXT("Attack04 pose reference %.2f s: observed montage %.3f s at +%.3f game s since Attack01"),
							Marker, Position, Elapsed - FirstAction));
					}
				}
			}
			LastLocalTime = Position;
		}
		if (bDown)
		{
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Released, 0.0f));
			bDown = false;
		}
		else if (Stage < 4 && Elapsed >= 0.120 * Presses)
		{
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Pressed, 1.0f));
			bDown = true;
			++Presses;
		}
		return false;
	},
		// 记录四段连点超时，以便定位最后成功进入的动作。
		[this]() { AddError(TEXT("Timed out waiting for the four-hit J chain")); return true; }, 20.0f));
	// 释放输入并还原角色位置，四段累积运动不应把后续独立用例带离训练平台。
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([Player, Fighter, Combat, InitialLocation]()
	{
		if (Player.IsValid() && Fighter.IsValid() && Combat.IsValid())
		{
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Released, 0.0f));
			Combat->InterruptActiveAction();
			Fighter->SetActorLocation(InitialLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}, 0.0f));
	return true;
}

#endif
