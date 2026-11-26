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
    const UCombatMoveCancelWindowTask* MoveCancel = nullptr;
    const UCombatBasicFollowUpTask* Basic = nullptr;
    const UCombatUltimateWindowTask* Ultimate = nullptr;
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
        else if (const auto* MovementWindow = Cast<UCombatMoveCancelWindowTask>(Task))
        {
            if (MoveCancel || !FMath::IsFinite(MovementWindow->BlendOutSeconds) || MovementWindow->BlendOutSeconds < 0.f)
            {
                return false;
            }
            MoveCancel = MovementWindow;
            Resolved.MoveBlendOutSeconds = MoveCancel->BlendOutSeconds;
        }
        else if (const auto* FollowUp = Cast<UCombatBasicFollowUpTask>(Task))
        {
            if (Basic)
            {
                return false;
            }
            Basic = FollowUp;
        }
        else if (const auto* InterruptWindow = Cast<UCombatUltimateWindowTask>(Task))
        {
            if (Ultimate)
            {
                return false;
            }
            Ultimate = InterruptWindow;
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
        || !ReadWindow(Chain, Resolved.StartPosition, EndPosition, Resolved.ChainWindow)
        || !ReadWindow(MoveCancel, Resolved.StartPosition, EndPosition, Resolved.MoveCancelWindow))
    {
        return false;
    }
    // 只配置一个输入窗口时，在该区间内即按即切，无需单独配置跳转。
    if (!Chain)
    {
        Resolved.ChainWindow = Resolved.InputBuffer;
    }
    // 普通动作沿用实际切招窗口，受完整播放保护的动作依赖单独配置。
    if (!MoveCancel && !bRequireFullPlayback)
    {
        Resolved.MoveCancelWindow = Resolved.ChainWindow;
        Resolved.MoveBlendOutSeconds = GetDefault<UCombatMoveCancelWindowTask>()->BlendOutSeconds;
    }
    if (Basic)
    {
        if (!(Basic->BufferStartSeconds >= 0.f && Basic->BufferStartSeconds >= Resolved.StartPosition - UE_KINDA_SMALL_NUMBER
            && Basic->BufferStartSeconds <= Basic->ChainStartSeconds && Basic->ChainStartSeconds < Basic->EndSeconds
            && Basic->EndSeconds <= EndPosition + UE_KINDA_SMALL_NUMBER))
        {
            return false;
        }
        Resolved.BasicFollowUpBuffer = { Basic->BufferStartSeconds, Basic->ChainStartSeconds };
        Resolved.BasicFollowUpChain = { Basic->ChainStartSeconds, Basic->EndSeconds };
    }
    if (Ultimate)
    {
        float Origin = 0.f;
        float Limit = EndPosition;
        if (!Ultimate->RelativeToSection.IsNone())
        {
            const int32 Index = Resolved.Montage->GetSectionIndex(Ultimate->RelativeToSection);
            if (Index == INDEX_NONE)
            {
                return false;
            }
            Resolved.Montage->GetSectionStartAndEndTime(Index, Origin, Limit);
        }
        FCombatAbilityWindow LocalWindow;
        if (!FMath::IsFinite(Ultimate->BufferLeadSeconds) || Ultimate->BufferLeadSeconds < 0.f
            || !ReadWindow(Ultimate, FMath::Max(0.f, Resolved.StartPosition - Origin),
                           FMath::Min(Limit, EndPosition) - Origin, LocalWindow))
        {
            return false;
        }
        const float Start = Origin + LocalWindow.StartSeconds;
        Resolved.UltimateChain = { Start, Origin + LocalWindow.EndSeconds };
        Resolved.UltimateBuffer = { FMath::Max(FMath::Max(Origin, Resolved.StartPosition), Start - Ultimate->BufferLeadSeconds), Start };
    }
    OutTasks = Resolved;
    return true;
}
