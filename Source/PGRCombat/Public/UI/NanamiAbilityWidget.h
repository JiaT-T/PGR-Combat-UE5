#pragma once

#include "Blueprint/UserWidget.h"
#include "NanamiAbilityWidget.generated.h"

class UImage;
class UNanamiOrbComponent;
class UTextBlock;
struct FNanamiOrbState;

UCLASS(Abstract)
class PGRCOMBAT_API UNanamiAbilityWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    // 视口创建或重新显示界面时绑定本地玩家，并读取当前角色的球状态。
    virtual void NativeConstruct() override;
    // 界面退出时解除控制器和角色的委托。
    virtual void NativeDestruct() override;

private:
    // 切换到新角色的状态源，离开七实时收起她的专属界面。
    UFUNCTION()
    void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
    // 用同一次结算后的快照刷新球图标、库存、三消提示和双星。
    UFUNCTION()
    void RefreshOrbs(const FNanamiOrbState& State);

    TWeakObjectPtr<APlayerController> ControllerSource;
    TWeakObjectPtr<UNanamiOrbComponent> OrbSource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> RedOrbIcon;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> RedOrbAlternate;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> RedOrbTriplePing;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> RedOrbStock;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> BlueOrbIcon;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> BlueOrbAlternate;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> BlueOrbTriplePing;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> BlueOrbStock;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> SignalStockTotal;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CoreStarTop;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CoreStarBottom;
};
