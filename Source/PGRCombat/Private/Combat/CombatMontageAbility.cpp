#include "Combat/CombatMontageAbility.h"

#include "Animation/AnimMontage.h"

namespace
{
    // 窗口使用 Montage 的绝对播放位置，且必须完整落在本次实际播放的范围内。
    bool ReadWindow(const UCombatWindowTask* Task, float Start, float End, FCombatAbilityWindow& OutWindow)
    {
        if (!Task)
        {
            return true;
        }
        if (!(Task->StartSeconds >= Start - UE_KINDA_SMALL_NUMBER && Task->StartSeconds >= 0.f
            && Task->StartSeconds < Task->EndSeconds && Task->EndSeconds <= End + UE_KINDA_SMALL_NUMBER))
        {
            return false;
        }
        OutWindow = { Task->StartSeconds, Task->EndSeconds };
        return true;
    }
}

// 允许设计者增删和重排 Task；重复、空项或越界配置在替换旧动作之前被拒绝。
bool UCombatMontageAbility::ResolveTasks(FCombatMontageTasks& OutTasks) const
{
    const UCombatInputBufferTask* Buffer = nullptr;
    const UCombatChainWindowTask* Chain = nullptr;
    FCombatMontageTasks Resolved;
    Resolved.bRequireFullPlayback = bRequireFullPlayback;
    for (const UCombatAbilityTask* Task : Tasks)
    {
        if (const auto* Play = Cast<UCombatPlayMontageTask>(Task))
        {
            if (Resolved.Montage || !Play->Montage || !FMath::IsFinite(Play->PlayRate) || Play->PlayRate <= 0.f)
            {
                return false;
            }
            Resolved.Montage = Play->Montage;
            Resolved.PlayRate = Play->PlayRate;
            Resolved.Section = Play->Section;
            Resolved.ExitSection = Play->ExitSection;
        }
        else if (const auto* Input = Cast<UCombatInputBufferTask>(Task))
        {
            if (Buffer)
            {
                return false;
            }
            Buffer = Input;
        }
        else if (const auto* Window = Cast<UCombatChainWindowTask>(Task))
        {
            if (Chain)
            {
                return false;
            }
            Chain = Window;
        }
        else
        {
            return false;
        }
    }
    if (!Resolved.Montage)
    {
        return false;
    }
    float EndPosition = Resolved.Montage->GetPlayLength();
    if (!Resolved.Section.IsNone())
    {
        const int32 Index = Resolved.Montage->GetSectionIndex(Resolved.Section);
        if (Index == INDEX_NONE || !Resolved.ExitSection.IsNone())
        {
            return false;
        }
        Resolved.Montage->GetSectionStartAndEndTime(Index, Resolved.StartPosition, EndPosition);
    }
    if (!Resolved.ExitSection.IsNone() && Resolved.Montage->GetSectionIndex(Resolved.ExitSection) == INDEX_NONE)
    {
        return false;
    }
    if (!ReadWindow(Buffer, Resolved.StartPosition, EndPosition, Resolved.InputBuffer)
        || !ReadWindow(Chain, Resolved.StartPosition, EndPosition, Resolved.ChainWindow))
    {
        return false;
    }
    // 只配置一个输入窗口时，在该区间内即按即切，无需单独配置跳转。
    if (!Chain)
    {
        Resolved.ChainWindow = Resolved.InputBuffer;
    }
    OutTasks = Resolved;
    return true;
}
