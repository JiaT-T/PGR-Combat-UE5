#include "UI/NanamiAbilityWidget.h"

#include "Combat/NanamiOrbComponent.h"
#include "Combat/NanamiFormComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
    // 把只读 HUD 元素切换为可见或隐藏，保持游戏视口接收点击。
    ESlateVisibility DisplayWhen(bool bDisplayed)
    {
        return bDisplayed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    }
}

// 先监听换人，再显示控制器此刻持有的角色状态。
void UNanamiAbilityWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ControllerSource = GetOwningPlayer();
    if (APlayerController* Player = ControllerSource.Get())
    {
        Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChanged);
        OnPossessedPawnChanged(nullptr, Player->GetPawn());
    }
}

// 从创建时的控制器和最近绑定的角色解除监听。
void UNanamiAbilityWidget::NativeDestruct()
{
    if (APlayerController* Player = ControllerSource.Get())
    {
        Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChanged);
    }
    if (UNanamiOrbComponent* Orbs = OrbSource.Get())
    {
        Orbs->OnOrbStateChanged.RemoveDynamic(this, &ThisClass::RefreshOrbs);
    }
    ControllerSource.Reset();
    OrbSource.Reset();
    if (UNanamiFormComponent* Form = FormSource.Get())
    {
        Form->OnWeaponFormChanged.RemoveDynamic(this, &ThisClass::RefreshForm);
    }
    FormSource.Reset();
    Super::NativeDestruct();
}

// 解除上一角色的状态源并立即同步新实例，避免重生后继续显示旧库存。
void UNanamiAbilityWidget::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    if (UNanamiOrbComponent* Previous = OrbSource.Get())
    {
        Previous->OnOrbStateChanged.RemoveDynamic(this, &ThisClass::RefreshOrbs);
    }
    UNanamiOrbComponent* Orbs = NewPawn ? NewPawn->FindComponentByClass<UNanamiOrbComponent>() : nullptr;
    if (UNanamiFormComponent* Previous = FormSource.Get())
    {
        Previous->OnWeaponFormChanged.RemoveDynamic(this, &ThisClass::RefreshForm);
    }
    FormSource = NewPawn ? NewPawn->FindComponentByClass<UNanamiFormComponent>() : nullptr;
    if (UNanamiFormComponent* Form = FormSource.Get())
    {
        Form->OnWeaponFormChanged.AddUniqueDynamic(this, &ThisClass::RefreshForm);
        RefreshForm(Form->GetWeaponForm());
    }
    OrbSource = Orbs;
    SetVisibility(DisplayWhen(Orbs != nullptr));
    if (Orbs)
    {
        Orbs->OnOrbStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshOrbs);
        RefreshOrbs(Orbs->GetOrbState());
    }
}

// 使用当前拥有角色的持械形态选择一张黄球图。
void UNanamiAbilityWidget::RefreshForm(ENanamiWeaponForm Form)
{
    if (YellowOrbIcon && YellowOrbAlternate)
    {
        YellowOrbIcon->SetVisibility(DisplayWhen(Form == ENanamiWeaponForm::SwordShield));
        YellowOrbAlternate->SetVisibility(DisplayWhen(Form == ENanamiWeaponForm::ShieldAxe));
    }
}

// 分色显示二段和一球三消，库存与亮星数量直接取自同一份快照。
void UNanamiAbilityWidget::RefreshOrbs(const FNanamiOrbState& State)
{
    RedOrbIcon->SetVisibility(DisplayWhen(!State.bRedFollowUp));
    RedOrbAlternate->SetVisibility(DisplayWhen(State.bRedFollowUp));
    RedOrbTriplePing->SetVisibility(DisplayWhen(State.bRedTriplePing));
    BlueOrbIcon->SetVisibility(DisplayWhen(!State.bBlueFollowUp));
    BlueOrbAlternate->SetVisibility(DisplayWhen(State.bBlueFollowUp));
    BlueOrbTriplePing->SetVisibility(DisplayWhen(State.bBlueTriplePing));

    RedOrbStock->SetText(FText::AsNumber(State.RedStock));
    BlueOrbStock->SetText(FText::AsNumber(State.BlueStock));
    SignalStockTotal->SetText(FText::Format(NSLOCTEXT("NanamiAbilityHUD", "OrbStock", "{0} / {1}"),
        FText::AsNumber(State.RedStock + State.BlueStock), FText::AsNumber(UNanamiOrbComponent::MaxStock)));
    // 实机只剩一颗时，点亮的是下方星位。
    CoreStarBottom->SetVisibility(DisplayWhen(State.Stars >= 1));
    CoreStarTop->SetVisibility(DisplayWhen(State.Stars >= 2));
}
