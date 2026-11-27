#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/CombatComponent.h"
#include "Combat/NanamiFormComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDodgeVfxPlaytest, "PGR.Local.Vfx.Dodge", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDodgeVfxPlaytest::RunTest(const FString&)
{
    UWorld* World = AutomationCommon::GetAnyGameWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    ACharacter* Pawn = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    UCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
    UNanamiFormComponent* Form = Pawn ? Pawn->FindComponentByClass<UNanamiFormComponent>() : nullptr;
    if (!TestTrue(TEXT("Training pawn ready"), PC && Pawn && Combat && Form)) return false;
    PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
    FString Folder;
    FParse::Value(FCommandLine::Get(), TEXT("DodgeMovie="), Folder);
    if (!Folder.IsEmpty()) IFileManager::Get().MakeDirectory(*Folder, true);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,PC,Pawn,Combat,Form,Folder,
        Start=-1., Due=-1., Pressed=-1., Phase=0, Shots=0, bSeen=false, bYellow=false, LastShot=-1.]() mutable
    {
        const double Now = Pawn->GetWorld()->GetTimeSeconds();
        auto Key = [PC](FKey K,bool Down) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f)); };
        if (Start < 0) { Start=Now; Due=Now+.35; }
        if (Now-Start>16) { AddError(FString::Printf(TEXT("Dodge timeout phase %d"),Phase)); return true; }
        if (!Folder.IsEmpty() && (LastShot<0 || Now-LastShot>=.032))
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString::Printf(TEXT("f%04d.png"),Shots++)),false,false);
            LastShot=Now;
        }
        if (Phase==2 && !bYellow)
        {
            if (!Combat->GetActiveActionId().IsNone()) return false;
            Key(EKeys::Three,true); bYellow=true; Due=Now+.5;
            return false;
        }
        if (Phase==2 && bYellow && Form->GetWeaponForm()!=ENanamiWeaponForm::ShieldAxe)
        {
            Key(EKeys::Three,false); return false;
        }
        if (Pressed<0 && Now>=Due && Combat->GetActiveActionId().IsNone())
        {
            const bool bTravel=(Phase&1)!=0;
            if (bTravel) Key(EKeys::D,true);
            Key(EKeys::LeftShift,true); Pressed=Now;
        }
        else if (Pressed>=0)
        {
            Key(EKeys::LeftShift,false);
            const bool bTravel=(Phase&1)!=0;
            const FName Expected(bTravel?TEXT("DodgeTravel"):TEXT("DodgeBack"));
            if (!bSeen && Combat->GetActiveActionId()==Expected)
            {
                const bool bAxe=Form->GetWeaponForm()==ENanamiWeaponForm::ShieldAxe;
                TestEqual(TEXT("Weapon form matches"),bAxe,Phase>=2);
                UE_LOG(LogTemp,Display,TEXT("PGR_DODGE_TEST phase=%d action=%s shot=%d"),Phase,*Expected.ToString(),Shots);
                bSeen=true;
            }
            if (bTravel && Now-Pressed>.08) Key(EKeys::D,false);
            if (bSeen && Combat->GetActiveActionId().IsNone())
            {
                ++Phase; Pressed=-1.; Due=Now+.35; bSeen=false;
                if(Phase==4) return true;
            }
        }
        return false;
    }));
    return true;
}
#endif

