#include "Combat/NanamiFormComponent.h"

#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Motion/CombatMotionComponent.h"
#include "TimerManager.h"

namespace
{
    constexpr float RequiredStarlight = 50.f;
    const FName SwordToAxe(TEXT("YellowSwordToAxe"));
    const FName AxeToSword(TEXT("YellowAxeToSword"));
    const FName YellowHold(TEXT("YellowHold"));
    const FName AxeUltimate(TEXT("Attack52"));
}

// 平时关闭组件 Tick，在焚冲和盾斧必杀期间跟踪收势。
UNanamiFormComponent::UNanamiFormComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 初始化角色资源，监听经播放组件确认过的动作实例。
void UNanamiFormComponent::BeginPlay()
{
    Super::BeginPlay();
    Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
    Motion = GetOwner()->FindComponentByClass<UCombatMotionComponent>();
    Starlight = FMath::Clamp(InitialStarlight, 0.f, 100.f);
    if (Combat)
    {
        Combat->OnActionChanged.AddUniqueDynamic(this, &ThisClass::OnActionChanged);
    }
}

// 离开关卡后，未到期的长按和旧动作都不再回调本组件。
void UNanamiFormComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
    if (Combat)
    {
        Combat->OnActionChanged.RemoveDynamic(this, &ThisClass::OnActionChanged);
    }
    Super::EndPlay(Reason);
}

// 达到门槛即可点黄切盾斧，星芒不足时仅允许在配置的训练模式中练习切形。
bool UNanamiFormComponent::HasStarlightForSwordToAxe() const
{
    if (Starlight >= RequiredStarlight)
    {
        return true;
    }

    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    const UClass* TrainingMode = StarlightFreeTapTrainingMode.Get();
    return TrainingMode && GameState && GameState->GameModeClass.Get() == TrainingMode;
}

// 剑盾首次发动需要五十星芒；上一轮留下的余量仅可用于重新按下后发动。
bool UNanamiFormComponent::CanStartYellowHold() const
{
    return WeaponForm == ENanamiWeaponForm::SwordShield
        && (Starlight >= RequiredStarlight || (bCanSpendRemainder && Starlight > 0.f));
}

// 红蓝二段和之后的资源来源共用同一份星芒存量。
void UNanamiFormComponent::AddStarlight(float Amount)
{
    if (FMath::IsFinite(Amount) && Amount > 0.f)
    {
        Starlight = FMath::Min(100.f, Starlight + Amount);
    }
}

// 扣除星芒并在归零时立即终止正在进行的焚冲攻击段。
void UNanamiFormComponent::ConsumeStarlight(float Amount)
{
    if (FMath::IsFinite(Amount) && Amount > 0.f)
    {
        Starlight = FMath::Max(0.f, Starlight - Amount);
        if (Starlight == 0.f)
        {
            bCanSpendRemainder = false;
            RequestYellowExit();
        }
    }
}

// 焚冲攻击段按时间耗星，盾斧必杀在普攻缓冲开启时回切剑盾。
void UNanamiFormComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!Motion)
    {
        return;
    }

    if (UltimateReturnInstanceId != 0)
    {
        if (Combat && Combat->GetActiveInstanceId() == UltimateReturnInstanceId
            && Motion->GetBasicFollowUpWindow() != ECombatChainWindow::Closed)
        {
            SetWeaponForm(ENanamiWeaponForm::SwordShield);
            SetComponentTickEnabled(false);
        }
        return;
    }
    if (Motion->IsBeforeActionSection(Motion->GetExitSection()))
    {
        ConsumeStarlight(StarlightDrainPerSecond * DeltaTime);
    }
    else if (IsYellowHoldExitSection(Motion->GetCurrentActionSection()))
    {
        SetWeaponForm(ENanamiWeaponForm::SwordShield);
        SetComponentTickEnabled(false);
    }
}

// 校验当前焚冲实例并匹配其 Ability 指定的收势段。
bool UNanamiFormComponent::IsYellowHoldExitSection(FName Section) const
{
    return HoldInstanceId != 0 && Motion && !Section.IsNone() && Section == Motion->GetExitSection();
}

// 每次真正的按下只建立一个计时器，收刀期间忽略新的手势。
void UNanamiFormComponent::PressYellow()
{
    if (!Combat || Combat->IsActionLocked() || bYellowPressed || HoldInstanceId != 0)
    {
        return;
    }
    bYellowPressed = true;
    bHoldThresholdReached = false;
    GetWorld()->GetTimerManager().SetTimer(HoldTimer, this, &ThisClass::OnHoldThreshold, HoldThresholdSeconds, false);
}

// 一旦识别为长按，本次松手便不再产生短按，即使本轮因形态或资源被拒绝。
void UNanamiFormComponent::OnHoldThreshold()
{
    if (bYellowPressed && Combat)
    {
        bHoldThresholdReached = true;
        Combat->SubmitInput(ECombatCommand::YellowOrb, ECombatInputPhase::Held);
    }
}

// 阈值前松手发动切形，阈值后松手仅使当前焚冲收刀。
void UNanamiFormComponent::ReleaseYellow()
{
    if (!bYellowPressed)
    {
        return;
    }
    bYellowPressed = false;
    GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
    if (bHoldThresholdReached)
    {
        RequestYellowExit();
    }
    else if (Combat)
    {
        Combat->SubmitInput(ECombatCommand::YellowOrb, ECombatInputPhase::Released);
    }
}

// 映射移除或失去控制权时丢弃手势，不产生切形短按。
void UNanamiFormComponent::CancelYellow()
{
    bYellowPressed = false;
    GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
    RequestYellowExit();
}

// 仅从仍属本轮的攻击区间跳转，收刀期间重复松手或耗尽不重播。
void UNanamiFormComponent::RequestYellowExit()
{
    if (HoldInstanceId != 0 && Combat && Combat->GetActiveInstanceId() == HoldInstanceId && Motion)
    {
        Motion->JumpForwardToActionSection(Motion->GetExitSection());
    }
}

// 同步切形、焚冲和盾斧必杀的已确认实例，并在对应实例退出时收回武器。
void UNanamiFormComponent::OnActionChanged(FName ActionId, int64 InstanceId, ECombatActionEvent Event)
{
    if (Event == ECombatActionEvent::Started && Combat->GetActiveInstanceId() == InstanceId)
    {
        if (ActionId == AxeUltimate)
        {
            UltimateReturnInstanceId = InstanceId;
            SetComponentTickEnabled(true);
        }
        else if (ActionId == YellowHold)
        {
            HoldInstanceId = InstanceId;
            SetComponentTickEnabled(true);
            SetWeaponForm(ENanamiWeaponForm::ShieldAxe);
        }
        else if (ActionId == SwordToAxe)
        {
            SetWeaponForm(ENanamiWeaponForm::ShieldAxe);
        }
        else if (ActionId == AxeToSword)
        {
            SetWeaponForm(ENanamiWeaponForm::SwordShield);
        }
    }
    else if (ActionId == AxeUltimate && UltimateReturnInstanceId != 0 && InstanceId == UltimateReturnInstanceId
        && Event != ECombatActionEvent::Rejected)
    {
        UltimateReturnInstanceId = 0;
        SetWeaponForm(ENanamiWeaponForm::SwordShield);
        SetComponentTickEnabled(false);
    }
    else if (ActionId == YellowHold && HoldInstanceId != 0 && InstanceId == HoldInstanceId
        && Event != ECombatActionEvent::Rejected)
    {
        HoldInstanceId = 0;
        bCanSpendRemainder = Starlight > 0.f;
        SetComponentTickEnabled(false);
        SetWeaponForm(ENanamiWeaponForm::SwordShield);
    }
}

// 武器和界面从同一份已生效的形态更新。
void UNanamiFormComponent::SetWeaponForm(ENanamiWeaponForm NewForm)
{
    if (WeaponForm != NewForm)
    {
        WeaponForm = NewForm;
        OnWeaponFormChanged.Broadcast(NewForm);
    }
}
