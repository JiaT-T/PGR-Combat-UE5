#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRBasicAttackTest, "PGR.Combat.BasicAttack.TrainingMap",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 在训练场验证按键出招、自然收招，以及同帧打断后再次出招。
bool FPGRBasicAttackTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!TestNotNull(TEXT("Training map has a possessed pawn"), Pawn))
	{
		return false;
	}

	TWeakObjectPtr<UCombatComponent> Combat = Pawn->FindComponentByClass<UCombatComponent>();
	if (!TestNotNull(TEXT("Player pawn owns the combat component"), Combat.Get()))
	{
		return false;
	}
	const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(Player->PlayerInput);
	if (!TestNotNull(TEXT("Player uses Enhanced Input"), Input))
	{
		return false;
	}
	TestTrue(TEXT("Training input maps J"), Input->GetEnhancedActionMappingsView().ContainsByPredicate(
		// 找出实际绑定到测试按键的输入映射。
		[](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Key == EKeys::J; }));

	TWeakObjectPtr<APlayerController> Controller = Player;
	// 向训练场的玩家控制器发送一次模拟的 J 键按下或松开事件。
	auto SendKey = [Controller](EInputEvent Event)
	{
		if (Controller.IsValid())
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
				EKeys::J, Event, Event == IE_Pressed ? 1.f : 0.f));
		}
	};

	// 从训练场的真实按键映射进入。第二次起手同时验证上一段自然结束后已经交还控制权。
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		// 在新的游戏帧按下普攻键。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([SendKey]() { SendKey(IE_Pressed); }));
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			// 等待脚本选出的首段动作进入活动状态。
			[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId() == TEXT("Attack01"); },
			// 超时表示输入没有触发首段，记录失败并结束等待。
			[this]() { AddError(TEXT("J did not start Attack01")); return true; }));
		// 松开按键，使下一次按下能重新触发 Started。
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([SendKey]() { SendKey(IE_Released); }));
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			// 等待自然收招归还角色控制权。
			[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId().IsNone(); },
			// 标记动作未在等待时限内收招。
			[this]() { AddError(TEXT("Attack01 did not finish")); return true; }));
	}

	// 同帧打断并重播同一个 Montage。下一帧旧实例的回调不能结束新实例。
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Combat]()
	{
		if (!Combat.IsValid())
		{
			AddError(TEXT("Combat component was destroyed before interruption test"));
			return;
		}
		// 在同一帧内提交普攻按下指令，用于比较打断前后的实例。
		const auto Press = [&Combat]()
		{
			return Combat->SubmitInput(ECombatCommand::BasicAttack, ECombatInputPhase::Pressed);
		};
		TestTrue(TEXT("Start before interrupt"), Press() == ECombatInputResult::Started);
		const int64 PreviousInstance = Combat->GetActiveInstanceId();
		Combat->InterruptActiveAction();
		TestTrue(TEXT("Restart after interrupt"), Press() == ECombatInputResult::Started);
		TestTrue(TEXT("Restart has a fresh instance"), Combat->GetActiveInstanceId() > PreviousInstance);
	}));
	// 给旧实例的结束回调留出一帧以上，再确认新实例仍在播放。
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Combat]()
	{
		TestTrue(TEXT("Replacement survives previous montage callback"),
		         Combat.IsValid() && Combat->GetActiveActionId() == TEXT("Attack01"));
	}, 0.05f));
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		// 等待替换后的动作独立完成。
		[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId().IsNone(); },
		// 为替换实例单独报告收招超时。
		[this]() { AddError(TEXT("Replacement Attack01 did not finish")); return true; }));
	return true;
}

#endif
