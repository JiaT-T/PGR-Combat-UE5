#pragma once

#include "Combat/CombatTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatScriptSubsystem.generated.h"

namespace puerts
{
    class FScriptEnv;
}

class UCombatComponent;
class UNanamiOrbComponent;

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(
    FName, FResolveCombatAction, UCombatComponent*, Combat, const FString&, Command, const FString&, Phase);

DECLARE_DYNAMIC_DELEGATE_ThreeParams(
    FNanamiOrbActionStarted, UNanamiOrbComponent*, Orbs, FName, ActionId, int64, InstanceId);

UCLASS()
class PGRCOMBAT_API UCombatScriptSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 为本次游戏实例创建并启动战斗脚本环境。
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // 在游戏实例销毁时关闭战斗脚本环境。
    virtual void Deinitialize() override;

    // 查询本次输入对应的动作，未选中时返回空标识。
    FName ResolveAction(UCombatComponent* Combat, ECombatCommand Command, ECombatInputPhase Phase) const;

    UPROPERTY()
    FResolveCombatAction ActionResolver;

    UPROPERTY()
    FNanamiOrbActionStarted NanamiOrbActionStarted;

    // 接收脚本的启动回执并校验选招入口。
    UFUNCTION()
    void ConfirmStartup();

private:
    TSharedPtr<puerts::FScriptEnv> ScriptEnvironment;
    bool bScriptReady = false;
};
