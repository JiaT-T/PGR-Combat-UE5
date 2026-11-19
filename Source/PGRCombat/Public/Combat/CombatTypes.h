#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

UENUM(BlueprintType)
enum class ECombatCommand : uint8
{
    BasicAttack
};

UENUM(BlueprintType)
enum class ECombatInputPhase : uint8
{
    Pressed,
    Released
};

UENUM(BlueprintType)
enum class ECombatInputResult : uint8
{
    Ignored,
    Rejected,
    Buffered,
    Started
};

UENUM(BlueprintType)
enum class ECombatActionEvent : uint8
{
    Started,
    Rejected,
    Completed,
    Interrupted
};
