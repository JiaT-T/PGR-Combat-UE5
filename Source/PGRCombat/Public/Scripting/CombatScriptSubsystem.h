#pragma once

#include "Combat/CombatTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatScriptSubsystem.generated.h"

namespace puerts
{
    class FScriptEnv;
}

class UCombatComponent;

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(
    FName, FResolveCombatAction, UCombatComponent*, Combat, const FString&, Command, const FString&, Phase);

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

    // 读取当前脚本启动所需的校验令牌。
    UFUNCTION()
    FString GetStartupToken() const;

    // 接收脚本的启动回执并校验选招入口。
    UFUNCTION()
    void ConfirmStartup(const FString& Reply);

private:
    TSharedPtr<puerts::FScriptEnv> ScriptEnvironment;
    FString StartupToken;
    bool bScriptReady = false;
};
