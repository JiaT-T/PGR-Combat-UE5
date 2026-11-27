#pragma once

#include "AnimNotify_PlayNiagaraEffect.h"
#include "PGRAnimNotify_WorldNiagaraOnce.generated.h"

UCLASS(meta = (DisplayName = "Play World Niagara Once (With Cleanup)"))
class UPGRAnimNotify_WorldNiagaraOnce : public UAnimNotify_PlayNiagaraEffect
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "AnimNotify", meta = (ClampMin = "0.01", Units = "s"))
    float CleanupAfter = 0.50f;

    // 沿用原生 Niagara 通知起播，按每次生成的组件独立安排清理。
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
