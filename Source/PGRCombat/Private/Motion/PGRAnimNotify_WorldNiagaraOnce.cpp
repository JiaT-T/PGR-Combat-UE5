#include "PGRAnimNotify_WorldNiagaraOnce.h"
#include "NanamiLocalVfxComponent.h"

#include "Engine/World.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

// 刀光脱离角色后按自己的时间收尾，超过可见窗口便销毁本次组件。
void UPGRAnimNotify_WorldNiagaraOnce::Notify(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (UNanamiLocalVfxComponent::ReplacesFirstHit(MeshComp, Animation))
    {
        return;
    }
    Super::Notify(MeshComp, Animation, EventReference);
    UFXSystemComponent* Effect = GetSpawnedEffect();
    UWorld* World = Effect ? Effect->GetWorld() : nullptr;
    if (World)
    {
        // 组件提前销毁或世界退出时，弱绑定保证定时器不会访问旧对象。
        FTimerHandle CleanupTimer;
        World->GetTimerManager().SetTimer(CleanupTimer, FTimerDelegate::CreateWeakLambda(Effect, [Effect]()
        {
            Effect->DestroyComponent();
        }), CleanupAfter, false);
    }
}
