#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Motion/CombatMotionComponent.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRNanamiFirstLinkTest, "PGR.Combat.BasicAttack.NanamiFirstLink",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 在七实训练地图验证真实按键续招、窗口外拒绝、重复输入和打断重置。
bool FPGRNanamiFirstLinkTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestTrue(TEXT("Run in Nanami's equipped training map"),
		World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/Training/Nanami/L_NanamiTraining")))
	{
		return false;
	}
	APlayerController* Player = World->GetFirstPlayerController();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const TWeakObjectPtr<UCombatComponent> Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	const TWeakObjectPtr<UCombatMotionComponent> Motion = Pawn ? Pawn->FindComponentByClass<UCombatMotionComponent>() : nullptr;
	if (!TestTrue(TEXT("Nanami has combat and motion components"), Combat.IsValid() && Motion.IsValid()))
	{
		return false;
	}

	// 排队等待运行时条件，超时时保留具体的失败步骤。
	const auto WaitFor = [this](const TCHAR* Step, TFunction<bool()> Poll)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(MoveTemp(Poll),
			// 记录当前等待步骤超时。
			[this, Step]() { AddError(Step); return true; }, 12.0f));
	};
	// 从玩家控制器输入一次真实映射的按下或松开事件。
	const auto SendKey = [Controller = TWeakObjectPtr<APlayerController>(Player)](EInputEvent Event)
	{
		if (Controller.IsValid())
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, Event, Event == IE_Pressed ? 1.f : 0.f));
		}
	};
	// 从统一战斗入口提交同帧按键，便于精确覆盖窗口边界和打断顺序。
	const auto Press = [Combat]()
	{
		return Combat.IsValid() ? Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed)
			: ECombatInputResult::Rejected;
	};
	// 首段只有进入真实 Montage 的输入窗口后才允许续招。
	const auto WindowOpen = [Combat, Motion]()
	{
		return Combat.IsValid() && Motion.IsValid() && Combat->GetActiveActionId() == TEXT("Attack01") && Motion->IsInputBufferOpen();
	};
	// 两次物理按键独立通过 Enhanced Input 和 TS，输出游戏时间供复核实际切换点。
	const TSharedRef<double> StartSeconds = MakeShared<double>(World->GetTimeSeconds());
	const TSharedRef<int64> WatchedInstance = MakeShared<int64>(0);
	SendKey(IE_Pressed);
	WaitFor(TEXT("First J press did not start Attack01"),
		// 记录首段起手并松键，为下一次按下重新布防。
		[Combat, SendKey, StartSeconds]()
		{
			if (!Combat.IsValid() || Combat->GetActiveActionId() != TEXT("Attack01"))
			{
				return false;
			}
			*StartSeconds = Combat->GetWorld()->GetTimeSeconds();
			SendKey(IE_Released);
			return true;
		});
	const TSharedRef<uint64> PressFrame = MakeShared<uint64>(0);
	WaitFor(TEXT("Second J did not link Attack01 to Attack02"),
		// 同一轮逐帧观察中先送出 J，再记录动作切换，避免固定延时推迟测量。
		[this, Combat, WindowOpen, SendKey, StartSeconds, PressFrame, WatchedInstance]()
		{
			if (!Combat.IsValid())
			{
				AddError(TEXT("Combat component destroyed during the controller input test"));
				return true;
			}
			const double Elapsed = Combat->GetWorld()->GetTimeSeconds() - *StartSeconds;
			if (*PressFrame == 0)
			{
				if (!WindowOpen())
				{
					return false;
				}
				*PressFrame = GFrameCounter;
				AddInfo(FString::Printf(TEXT("Second J press: frame %llu, %.3f game seconds since Attack01"), GFrameCounter, Elapsed));
				SendKey(IE_Pressed);
			}
			if (Combat->GetActiveActionId() != TEXT("Attack02"))
			{
				return false;
			}
			*WatchedInstance = Combat->GetActiveInstanceId();
			const uint64 DelayFrames = GFrameCounter - *PressFrame;
			AddInfo(FString::Printf(TEXT("Attack02 observed: frame %llu, %.3f game seconds since Attack01, %llu frames after J"), GFrameCounter, Elapsed, DelayFrames));
			TestTrue(TEXT("Attack02 is observable by the next player-input tick"), DelayFrames <= 1);
			return true;
		});
	// 释放实际输入，后续场景独立从空闲状态开始。
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([SendKey]() { SendKey(IE_Released); }, 0.0f));
	// 按序观察下一次起手前的所有活动实例。
	const auto ExpectIdle = [this, Combat, WatchedInstance, WaitFor](FName Expected)
	{
		WaitFor(TEXT("Attack did not return to idle"),
			// 被观察的动作只能继续原实例或归还控制权。
			[this, Combat, WatchedInstance, Expected]()
			{
				if (!Combat.IsValid())
				{
					AddError(TEXT("Combat component destroyed while waiting for idle"));
					return true;
				}
				if (Combat->GetActiveActionId().IsNone())
				{
					return true;
				}
				if (Combat->GetActiveActionId() != Expected || Combat->GetActiveInstanceId() != *WatchedInstance)
				{
					AddError(TEXT("Unexpected extra attack instance before idle"));
					return true;
				}
				return false;
			});
	};
	ExpectIdle(TEXT("Attack02"));

	// 单点和窗外连点各跑一轮，两者均只允许首段自然结束。
	for (const bool bEarlyPress : {false, true})
	{
		// 两轮的区别只在于起手时是否额外按一次普攻。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Combat, Motion, Press, WatchedInstance, bEarlyPress]()
		{
			if (!TestTrue(TEXT("Attack01 started"), Combat.IsValid() && Motion.IsValid() && Press() == ECombatInputResult::Started))
			{
				return;
			}
			*WatchedInstance = Combat->GetActiveInstanceId();
			if (bEarlyPress)
			{
				TestFalse(TEXT("Link window is closed at startup"), Motion->IsInputBufferOpen());
				TestTrue(TEXT("Early press was rejected"), Press() == ECombatInputResult::Rejected);
			}
		}));
		ExpectIdle(TEXT("Attack01"));
	}
	// 直接从战斗入口提交窗口内按键，检查函数返回前就已经换到第二段。
	for (const bool bInterrupt : {false, true})
	{
		// 让每轮续招各自从空闲状态开始。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Press]()
		{
			TestTrue(TEXT("Fresh Attack01 started"), Press() == ECombatInputResult::Started);
		}));
		WaitFor(TEXT("First-link window missing in repeat or interrupt case"),
			// 同一窗口内连点两次，按本轮场景决定是否立即中断。
			[this, Combat, WindowOpen, Press, WatchedInstance, bInterrupt]()
			{
				if (!WindowOpen())
				{
					return false;
				}
				const int64 FirstInstance = Combat->GetActiveInstanceId();
				TestTrue(TEXT("Window press starts the continuation immediately"), Press() == ECombatInputResult::Started);
				*WatchedInstance = Combat->GetActiveInstanceId();
				TestTrue(TEXT("Attack02 active when SubmitInput returns"),
					Combat->GetActiveActionId() == TEXT("Attack02") && *WatchedInstance > FirstInstance);
				Press();
				TestTrue(TEXT("Additional press keeps the same Attack02 instance"),
					Combat->GetActiveActionId() == TEXT("Attack02") && Combat->GetActiveInstanceId() == *WatchedInstance);
				if (bInterrupt)
				{
					const int64 OldInstance = Combat->GetActiveInstanceId();
					Combat->InterruptActiveAction();
					TestTrue(TEXT("Interrupt permitted a fresh start"), Press() == ECombatInputResult::Started);
					*WatchedInstance = Combat->GetActiveInstanceId();
					TestTrue(TEXT("Restart is a new Attack01"), Combat->GetActiveActionId() == TEXT("Attack01") && *WatchedInstance > OldInstance);
				}
				return true;
			});
		ExpectIdle(bInterrupt ? TEXT("Attack01") : TEXT("Attack02"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRNanamiTapCadenceTest, "PGR.Combat.BasicAttack.NanamiTapCadence",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 固定每 120 毫秒从控制器点按一次 J，验证连续点击的玩家能顺畅接出第二段。
bool FPGRNanamiTapCadenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	TWeakObjectPtr<APlayerController> Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	TWeakObjectPtr<UCombatComponent> Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!TestTrue(TEXT("Cadence test needs Nanami's training pawn"), Combat.IsValid()
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/Training/Nanami/L_NanamiTraining")))
	{
		return false;
	}

	// 输入时间只由游戏时钟决定；动作状态用于记录和核实结果。
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand([this, Controller, Combat, Start = -1.0, FirstAction = -1.0,
		LastPress = 0.0, PressCount = 0, bDown = false, bLinked = false]() mutable
	{
		if (!Controller.IsValid() || !Combat.IsValid())
		{
			AddError(TEXT("Lost the training pawn during repeated J presses"));
			return true;
		}
		const double Now = Controller->GetWorld()->GetTimeSeconds();
		if (Start < 0.0)
		{
			Start = Now;
		}
		const double Elapsed = Now - Start;
		const FName Action = Combat->GetActiveActionId();
		if (Action == TEXT("Attack01") && FirstAction < 0.0)
		{
			FirstAction = Elapsed;
		}
		if (Action == TEXT("Attack02") && !bLinked)
		{
			bLinked = true;
			AddInfo(FString::Printf(TEXT("J cadence 0.120 s: tap %d at %.3f s, Attack01 at %.3f s, Attack02 at %.3f s (%.3f s after Attack01)"),
				PressCount, LastPress, FirstAction, Elapsed, Elapsed - FirstAction));
			TestTrue(TEXT("Attack01 was observed before Attack02"), FirstAction >= 0.0 && FirstAction < Elapsed);
			TestTrue(TEXT("A new tap started Attack02 by the fourth press"), PressCount >= 2 && PressCount <= 4);
		}
		if (bDown)
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Released, 0.0f));
			bDown = false;
		}
		else if (!bLinked && Elapsed >= 0.120 * PressCount)
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Pressed, 1.0f));
			++PressCount;
			LastPress = Elapsed;
			bDown = true;
		}
		return bLinked && Action.IsNone();
	},
		// 超时后也松键，保持后续独立场景的输入初态。
		[this, Controller]()
		{
			if (Controller.IsValid())
			{
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Released, 0.0f));
			}
			AddError(TEXT("Repeated J presses did not complete the first link"));
			return true;
		}, 12.0f));
	return true;
}

#endif
