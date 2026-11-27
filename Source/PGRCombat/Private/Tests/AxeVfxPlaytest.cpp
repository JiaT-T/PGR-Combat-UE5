#if WITH_DEV_AUTOMATION_TESTS
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatComponent.h"
#include "Combat/NanamiFormComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAxeVfxPlaytest, "PGR.Local.AxeVfx.Combo", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 实机输入 3 换盾斧、点按 J 进入完整连段，录制实战镜头，不绕过战斗组件。
bool FAxeVfxPlaytest::RunTest(const FString& Parameters)
{
    UWorld* World = AutomationCommon::GetAnyGameWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    ACharacter* Pawn = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    UCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
    UNanamiFormComponent* Form = Pawn ? Pawn->FindComponentByClass<UNanamiFormComponent>() : nullptr;
    if (!TestTrue(TEXT("Training pawn is possessed"), PC && Pawn && Combat && Form))
    {
        return false;
    }
    PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
    for (int32 I = 0; I < 8; ++I)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown, IE_Pressed, 1.f));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown, IE_Released, 0.f));
    }
    FString Folder;
    FParse::Value(FCommandLine::Get(), TEXT("AxeVfxMovie="), Folder);
    if (!Folder.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*Folder, true);
    }
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, PC, Pawn, Combat, Form, Folder, NowStart = -1.0, Stamp = 0.0,
        First = -1.0, Last = -1.0, Stage = 0, Yellow = 0, Shots = 0, JDown = false]() mutable
    {
        const double Now = Pawn->GetWorld()->GetTimeSeconds();
        if (NowStart < 0.0) NowStart = Now;
        const auto Press = [PC](FKey Key, bool Down)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Down ? IE_Pressed : IE_Released, Down ? 1.f : 0.f));
        };
        if (Now - NowStart > 15)
        {
            AddError(FString::Printf(TEXT("Axe combo timeout at stage %d, action %s"), Stage, *Combat->GetActiveActionId().ToString()));
            return true;
        }
        if (Yellow == 0 && Now - NowStart > .65)
        {
            Press(EKeys::Three, true);
            Yellow = 1;
        }
        else if (Yellow == 1)
        {
            Press(EKeys::Three, false);
            Yellow = 2;
        }
        if (Yellow != 2 || Form->GetWeaponForm() != ENanamiWeaponForm::ShieldAxe)
        {
            return false;
        }
        const FName Action = Combat->GetActiveActionId();
        if (Stage == 0 && Action.IsNone() && First < 0)
        {
            First = Now;
            Stamp = Now;
        }
        if (First < 0) return false;
        const int32 Next = Stage + 1;
        if (Stage < 4 && Action == FName(*FString::Printf(TEXT("Attack%d"), 60 + Next)))
        {
            Stage++;
            Last = Now;
            const UAnimInstance* Anim = Pawn->GetMesh()->GetAnimInstance();
            const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
            TestEqual(TEXT("Axe montage follows the accepted attack"), GetNameSafe(Montage), TEXT("AM_Equipped") + Action.ToString());
            UE_LOG(LogTemp, Display, TEXT("AXE_GAMEPLAY_ACTION %s t=%.3f root=%s blade=%s"), *Action.ToString(), Now - First,
                *Pawn->GetMesh()->GetComponentLocation().ToCompactString(),
                *Pawn->GetMesh()->GetSocketLocation(TEXT("Bip001Prop2")).ToCompactString());
        }
        if (Action==TEXT("Attack62") || Action==TEXT("Attack64"))
        {
            const auto* Mesh=Pawn->GetMesh();
            const auto* Anim=Mesh->GetAnimInstance();
            const auto* Montage=Anim?Anim->GetCurrentActiveMontage():nullptr;
            if(Montage)
            {
                const FVector Base=Mesh->GetComponentLocation();
                const FVector Blade=Mesh->GetSocketLocation(TEXT("Bip001Prop2"));
                const FVector Hand=Mesh->GetSocketLocation(TEXT("Bip001 R Hand"));
                const FRotator W=Mesh->GetSocketRotation(TEXT("Bip001Prop2"));
                const FVector Rel=FQuat(FRotator(0,Pawn->GetActorRotation().Yaw,0)).UnrotateVector(Blade-Base);
                UE_LOG(LogTemp, Display,TEXT("AXE_PATH %s %.4f %.4f | %.1f %.1f %.1f | %.1f %.1f %.1f | %.1f %.1f %.1f | %.1f %.1f %.1f | %.1f | %.1f %.1f %.1f"),
                    *Action.ToString(),Anim->Montage_GetPosition(Montage),Now-First,
                    Base.X,Base.Y,Base.Z,Blade.X,Blade.Y,Blade.Z,Rel.X,Rel.Y,Rel.Z,W.Pitch,W.Yaw,W.Roll,Pawn->GetActorRotation().Yaw,Hand.X,Hand.Y,Hand.Z);
            }
        }
        if (!Folder.IsEmpty() && Now + 1e-4 >= First + Shots / 30.0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("f%04d.png"), Shots)), false, false);
            ++Shots;
        }
        if (JDown)
        {
            Press(EKeys::J, false);
            JDown = false;
        }
        else if (Stage < 4 && Now >= Stamp)
        {
            Press(EKeys::J, true);
            JDown = true;
            Stamp = Now + .125;
        }
        if (Stage == 4 && Action.IsNone() && Now - Last > 1.12)
        {
            UE_LOG(LogTemp, Display, TEXT("AXE_GAMEPLAY_DONE shots=%d"), Shots);
            return true;
        }
        return false;
    }));
    return true;
}
#endif



#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtraVfxPlaytest, "PGR.Local.Vfx.ExtraOverlays", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// 依次通过真实输入触发剑盾四段和黄球往返形态，不直接调用特效。
bool FExtraVfxPlaytest::RunTest(const FString& Parameters)
{
    auto* World=AutomationCommon::GetAnyGameWorld();
    auto* PC=World?World->GetFirstPlayerController():nullptr;
    auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    auto* Combat=Pawn?Pawn->FindComponentByClass<UCombatComponent>():nullptr;
    if(!TestTrue(TEXT("Training combat input available"), Combat!=nullptr))return false;
    FString Folder;
    FParse::Value(FCommandLine::Get(), TEXT("AxeVfxMovie="), Folder);
    if(!Folder.IsEmpty())IFileManager::Get().MakeDirectory(*Folder,true);
    PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
    for(int i=0;i<6;++i)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Pressed,1));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Released,0));
    }
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,World,PC,Combat,Folder,Start=-1.0,Next=0.0,Since=0.0,Shots=0,Stage=0,Key=FKey()]()mutable
    {
        const double Now=World->GetTimeSeconds();
        if(Start<0)Start=Now;
        const double T=Now-Start;
        if(T>24)
        {
            AddError(FString::Printf(TEXT("Overlay input timed out stage=%d action=%s"),Stage,*Combat->GetActiveActionId().ToString()));return true;
        }
        const auto A=Combat->GetActiveActionId();
        if(Stage==0&&A==TEXT("Attack04"))
        {
            Stage=1;Since=T;UE_LOG(LogTemp,Display,TEXT("PGR_EXTRA_ACTION Sword04 at %.3f"),T);
        }
        if(Stage==1&&T-Since>1&&A.IsNone()){Stage=2;Next=T;}
        if(Stage==2&&A==TEXT("YellowSwordToAxe")){Stage=3;Since=T;}
        if(Stage==3&&T-Since>1.4&&A.IsNone()){Stage=4;Next=T;}
        if(Stage==4&&A==TEXT("YellowAxeToSword"))
        {
            Stage=5;Since=T;UE_LOG(LogTemp,Display,TEXT("PGR_EXTRA_ACTION YellowReturn at %.3f"),T);
        }
        if(!Folder.IsEmpty()&&T+1e-4>=Shots/30.0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString::Printf(TEXT("f%04d.png"),Shots++)),false,false);
        }
        if(Key.IsValid())
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0));Key=FKey();
        }
        else if(T>=Next&&(Stage==0||Stage==2||Stage==4))
        {
            Key=Stage==0?EKeys::J:EKeys::Three;Next=T+(Stage==0?.14:.4);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1));
        }
        if(Stage==5&&T-Since>1.35)
        {
            UE_LOG(LogTemp,Display,TEXT("PGR_EXTRA_OVERLAYS_DONE shots=%d"),Shots); return true;
        }
        return false;
    }));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRHoldVfxPlaytest, "PGR.Local.Vfx.HoldAndOrbs", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FPGRHoldVfxPlaytest::RunTest(const FString& Params)
{
    auto* W=AutomationCommon::GetAnyGameWorld();
    auto* PC=W?W->GetFirstPlayerController():nullptr;
    APawn* P=PC?PC->GetPawn():nullptr;
    auto* C=P?P->FindComponentByClass<UCombatComponent>():nullptr;
    auto* F=P?P->FindComponentByClass<UNanamiFormComponent>():nullptr;
    if(!TestTrue(TEXT("Training combat/form"),C&&F))return false;
    FString Folder;
    FParse::Value(FCommandLine::Get(), TEXT("AxeVfxMovie="), Folder);
    if(!Folder.IsEmpty())IFileManager::Get().MakeDirectory(*Folder,true);
    PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,W,PC,C,F,Folder,Start=-1.0,At=0.0,Stage=0,Shot=0]() mutable{
        const double Now=W->GetTimeSeconds();
        if(Start<0)Start=Now;
        const double T=Now-Start;
        const FName A=C->GetActiveActionId();
        const auto key=[PC](FKey K,bool Down){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));};
        const auto next=[&](){UE_LOG(LogTemp,Display,TEXT("PGR_VFX_E2E_STAGE %d %s %.3f"),Stage,*A.ToString(),T);++Stage;At=T;};
        if(T>40){AddError(FString::Printf(TEXT("Coverage stage %d stalled, action %s"),Stage,*A.ToString()));return true;}
        if(!Folder.IsEmpty() && T+.0001>=Shot/24.0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString::Printf(TEXT("f%04d.png"),Shot++)),false,false);
        }
        switch(Stage)
        {
        case 0: if(T>.7){key(EKeys::One,true);next();} break;
        case 1: if(A==TEXT("Attack11")){key(EKeys::One,false);next();} break;
        case 2: if(T-At>1.08){key(EKeys::One,true);next();} break;
        case 3: if(A==TEXT("Attack12")){key(EKeys::One,false);next();}else if(T-At>.12){key(EKeys::One,false);key(EKeys::One,true);At=T;} break;
        case 4: if(T-At>1.2&&A.IsNone()){key(EKeys::Two,true);next();} break;
        case 5: if(A==TEXT("Attack21")){key(EKeys::Two,false);next();} break;
        case 6: if(T-At>1.00){key(EKeys::Two,true);next();} break;
        case 7: if(A==TEXT("Attack22")){key(EKeys::Two,false);next();}else if(T-At>.12){key(EKeys::Two,false);key(EKeys::Two,true);At=T;} break;
        case 8: if(T-At>1.1&&A.IsNone()){key(EKeys::Three,true);next();} break;
        case 9: if(T-At>.06){key(EKeys::Three,false);next();} break;
        case 10: if(T-At>1.2&&A.IsNone()&&F->GetWeaponForm()==ENanamiWeaponForm::ShieldAxe){key(EKeys::J,true);next();} break;
        case 11: if(A==TEXT("Attack99")){key(EKeys::J,false);next();} break;
        case 12: if(T-At>1.9&&A.IsNone()){key(EKeys::Three,true);next();} break;
        case 13: if(A==TEXT("YellowAxeToSword")){key(EKeys::Three,false);next();} else if(T-At>.06){key(EKeys::Three,false);next();} break;
        case 14: if(T-At>1.7&&A.IsNone()&&F->GetWeaponForm()==ENanamiWeaponForm::SwordShield){key(EKeys::Three,true);next();} break;
        case 15: if(A==TEXT("YellowHold")){next();} break;
        case 16: if(T-At>3.25){key(EKeys::Three,false);next();} break;
        case 17: if(T-At>1.6&&A.IsNone()){UE_LOG(LogTemp,Display,TEXT("PGR_VFX_E2E_DONE frames=%d"),Shot);return true;} break;
        }
        return false;
    }));
    return true;
}
#endif


#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRUltVfxPlaytest, "PGR.Local.Vfx.UltimatesWorld", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FPGRUltVfxPlaytest::RunTest(const FString& Params)
{
    auto* W=AutomationCommon::GetAnyGameWorld();
    auto* PC=W?W->GetFirstPlayerController():nullptr;
    APawn* P=PC?PC->GetPawn():nullptr;
    auto* C=P?P->FindComponentByClass<UCombatComponent>():nullptr;
    auto* F=P?P->FindComponentByClass<UNanamiFormComponent>():nullptr;
    if(!TestTrue(TEXT("Training combat"),C&&F))return false;
    FString Folder;
    FParse::Value(FCommandLine::Get(), TEXT("AxeVfxMovie="), Folder);
    if(!Folder.IsEmpty())IFileManager::Get().MakeDirectory(*Folder,true);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,W,PC,C,F,Folder,Start=-1.0,At=0.0,Stage=0,Shot=0]() mutable{
        const double Now=W->GetTimeSeconds();
        if(Start<0)Start=Now;
        const double T=Now-Start;
        const FName A=C->GetActiveActionId();
        const auto key=[PC](FKey K,bool Down){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));};
        const auto next=[&](){UE_LOG(LogTemp,Display,TEXT("PGR_ULT_VFX_STAGE %d %s %.3f"),Stage,*A.ToString(),T);++Stage;At=T;};
        if(T>34){AddError(FString::Printf(TEXT("Ult effect stage %d stalled, action %s"),Stage,*A.ToString()));return true;}
        if(!Folder.IsEmpty() && T+.0001>=Shot/12.0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString::Printf(TEXT("f%04d.png"),Shot++)),false,false);
        }
        switch(Stage)
        {
        case 0: if(T>.70){key(EKeys::R,true);next();}break;
        case 1: if(A==TEXT("Attack51")){key(EKeys::R,false);next();}break;
        case 2: if(T-At>7&&A.IsNone()){key(EKeys::Three,true);next();}break;
        case 3: if(T-At>.06){key(EKeys::Three,false);next();}break;
        case 4: if(T-At>1.2&&A.IsNone()&&F->GetWeaponForm()==ENanamiWeaponForm::ShieldAxe){key(EKeys::R,true);next();}break;
        case 5: if(A==TEXT("Attack52")){key(EKeys::R,false);next();}break;
        case 6: if(T-At>7&&A.IsNone()){UE_LOG(LogTemp,Display,TEXT("PGR_ULT_VFX_DONE frames=%d"),Shot);return true;}break;
        }
        return false;
    }));
    return true;
}
#endif

