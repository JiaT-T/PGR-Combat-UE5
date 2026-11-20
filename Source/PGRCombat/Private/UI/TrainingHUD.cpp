#include "UI/TrainingHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

// 使用拥有该 HUD 的本地玩家创建界面，鼠标输入继续传给游戏视口。
void ATrainingHUD::BeginPlay()
{
    Super::BeginPlay();
    APlayerController* Player = GetOwningPlayerController();
    if (Player && Player->IsLocalPlayerController() && AbilityWidgetClass)
    {
        AbilityWidget = CreateWidget<UUserWidget>(Player, AbilityWidgetClass);
        if (AbilityWidget)
        {
            AbilityWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
            AbilityWidget->AddToPlayerScreen();
        }
    }
}

// 移除本 HUD 创建的控件，避免跨关卡或重建后重复叠加。
void ATrainingHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AbilityWidget)
    {
        AbilityWidget->RemoveFromParent();
        AbilityWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}
