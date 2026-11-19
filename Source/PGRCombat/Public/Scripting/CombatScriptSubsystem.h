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
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FName ResolveAction(UCombatComponent* Combat, ECombatCommand Command, ECombatInputPhase Phase) const;

    UPROPERTY()
    FResolveCombatAction ActionResolver;

    UFUNCTION()
    FString GetStartupToken() const;

    UFUNCTION()
    void ConfirmStartup(const FString& Reply);

private:
    TSharedPtr<puerts::FScriptEnv> ScriptEnvironment;
    FString StartupToken;
    bool bScriptReady = false;
};
