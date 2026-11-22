#include "Combat/CombatMontageAbility.h"

#include "Animation/AnimMontage.h"

namespace
{
    // 可选窗口缺席时保留空区间，已添加的窗口必须完整落在 Montage 上。
    bool ReadWindow(const UCombatWindowTask* Task, float Length, FCombatAbilityWindow& OutWindow)
    {
        if (!Task)
        {
            return true;
        }
        if (!(Task->StartSeconds >= 0.f && Task->StartSeconds < Task->EndSeconds
            && Task->EndSeconds <= Length + UE_KINDA_SMALL_NUMBER))
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
    for (const UCombatAbilityTask* Task : Tasks)
    {
        if (const auto* Play = Cast<UCombatPlayMontageTask>(Task))
        {
            if (Resolved.Montage || !Play->Montage)
            {
                return false;
            }
            Resolved.Montage = Play->Montage;
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
    const float Length = Resolved.Montage->GetPlayLength();
    if (!ReadWindow(Buffer, Length, Resolved.InputBuffer) || !ReadWindow(Chain, Length, Resolved.ChainWindow))
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
