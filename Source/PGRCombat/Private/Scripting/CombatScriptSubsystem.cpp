#include "Scripting/CombatScriptSubsystem.h"

#include "Combat/CombatComponent.h"
#include "Misc/Guid.h"
#include "PapiBackendQuickjs.h"
#include "ScriptEnv.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRScript, Log, All);

// 启动 QuickJS 战斗入口，并通过一次原生往返确认脚本已就绪。
void UCombatScriptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    StartupToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ScriptEnvironment = MakeShared<puerts::FScriptEnv>(&puerts::FPapiBackendQuickjs::Create, TEXT("JavaScript"));

    TArray<TPair<FString, UObject*>> Arguments;
    Arguments.Emplace(TEXT("CombatHost"), this);
    ScriptEnvironment->Start(TEXT("PGR/main.js"), Arguments);
    StartupToken.Reset();

    if (bScriptReady)
    {
        UE_LOG(LogPGRScript, Display, TEXT("Puerts bridge ready: QuickJS PGR/main.js completed native round trip"));
    }
    else
    {
        UE_LOG(LogPGRScript, Error, TEXT("Puerts bridge startup failed: PGR/main.js did not complete native round trip"));
        ScriptEnvironment.Reset();
    }
}

// 向脚本返回本次启动校验所用的临时令牌。
FString UCombatScriptSubsystem::GetStartupToken() const
{
    return StartupToken;
}

// 核对脚本注册的选招回调和返回令牌，通过后允许脚本处理输入。
void UCombatScriptSubsystem::ConfirmStartup(const FString& Reply)
{
    if (ActionResolver.IsBound() && !StartupToken.IsEmpty() &&
        Reply.Equals(FString(TEXT("PGR/")) + StartupToken, ESearchCase::CaseSensitive))
    {
        bScriptReady = true;
    }
}

// 将输入指令和按键阶段转换为枚举名称，交给脚本解析为动作标识。
FName UCombatScriptSubsystem::ResolveAction(UCombatComponent* Combat, ECombatCommand Command, ECombatInputPhase Phase) const
{
    if (!bScriptReady || !ActionResolver.IsBound())
    {
        return NAME_None;
    }

    // 传枚举名，避免 TS 手写的数值顺序与 UE 枚举脱节。
    const FString CommandName = StaticEnum<ECombatCommand>()->GetNameStringByValue(static_cast<int64>(Command));
    const FString PhaseName = StaticEnum<ECombatInputPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
    return ActionResolver.Execute(Combat, CommandName, PhaseName);
}

// 解除选招回调并释放当前游戏实例的脚本运行环境。
void UCombatScriptSubsystem::Deinitialize()
{
    ActionResolver.Unbind();
    ScriptEnvironment.Reset();
    bScriptReady = false;
    UE_LOG(LogPGRScript, Display, TEXT("Puerts bridge stopped"));
    Super::Deinitialize();
}
