#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatScriptSubsystem.generated.h"

namespace puerts
{
    class FScriptEnv;
}

UCLASS()
class PGRCOMBAT_API UCombatScriptSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION()
    FString GetStartupToken() const;

    UFUNCTION()
    void ConfirmStartup(const FString& Reply);

private:
    TSharedPtr<puerts::FScriptEnv> ScriptEnvironment;
    FString StartupToken;
    bool bScriptReady = false;
};
