#pragma once

#include "GameFramework/HUD.h"
#include "TrainingHUD.generated.h"

class UUserWidget;

UCLASS()
class PGRCOMBAT_API ATrainingHUD : public AHUD
{
    GENERATED_BODY()

protected:
    // 将训练关卡指定的界面添加到本地玩家的视口。
    virtual void BeginPlay() override;
    // 关卡切换或 HUD 被替换时移除旧界面。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "Training|HUD")
    TSubclassOf<UUserWidget> AbilityWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> AbilityWidget;
};
