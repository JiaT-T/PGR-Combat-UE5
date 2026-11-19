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
		[](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Key == EKeys::J; }));

	TWeakObjectPtr<APlayerController> Controller = Player;
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
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([SendKey]() { SendKey(IE_Pressed); }));
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId() == TEXT("Attack01"); },
			[this]() { AddError(TEXT("J did not start Attack01")); return true; }));
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([SendKey]() { SendKey(IE_Released); }));
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId().IsNone(); },
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
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Combat]()
	{
		TestTrue(TEXT("Replacement survives previous montage callback"),
		         Combat.IsValid() && Combat->GetActiveActionId() == TEXT("Attack01"));
	}, 0.05f));
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[Combat]() { return Combat.IsValid() && Combat->GetActiveActionId().IsNone(); },
		[this]() { AddError(TEXT("Replacement Attack01 did not finish")); return true; }));
	return true;
}

#endif
