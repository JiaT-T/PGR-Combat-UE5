#include "Scripting/CombatScriptSubsystem.h"

#include "Combat/CombatComponent.h"
#include "Misc/Guid.h"
#include "PapiBackendQuickjs.h"
#include "ScriptEnv.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRScript, Log, All);

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

FString UCombatScriptSubsystem::GetStartupToken() const
{
    return StartupToken;
}

void UCombatScriptSubsystem::ConfirmStartup(const FString& Reply)
{
    if (ActionResolver.IsBound() && !StartupToken.IsEmpty() &&
        Reply.Equals(FString(TEXT("PGR/")) + StartupToken, ESearchCase::CaseSensitive))
    {
        bScriptReady = true;
    }
}

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

void UCombatScriptSubsystem::Deinitialize()
{
    ActionResolver.Unbind();
    ScriptEnvironment.Reset();
    bScriptReady = false;
    UE_LOG(LogPGRScript, Display, TEXT("Puerts bridge stopped"));
    Super::Deinitialize();
}
