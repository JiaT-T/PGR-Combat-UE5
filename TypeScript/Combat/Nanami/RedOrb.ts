export interface RedOrbState {
    readonly RedStock: number;
    readonly Stars: number;
    readonly bRedFollowUp: boolean;
    readonly bRedTriplePing: boolean;
}

export interface NanamiOrbComponent {
    // 读取当前角色信号球数据的一份快照。
    GetOrbState(): RedOrbState;
    // 读取训练场样例中普通红二段所选同色组的大小。
    GetTrainingRedGroupSize(): number;
    // 将本次动作的实际支出、有效消球档位和下一状态一起提交。
    ApplyRedOrbResult(instanceId: bigint, spentOrbs: number, effectivePings: number, spentStars: number,
                      followUp: boolean, triplePing: boolean): void;
}

// 仅在有红球可用时选招；二段资格独立保存在角色上，收招后也能读取。
export function resolveRedOrb(state: RedOrbState | undefined): string {
    if (!state || state.RedStock < 1) {
        return "";
    }

    return state.bRedFollowUp ? "Attack12" : "Attack11";
}

// 红一起播只开启续段；红二按角色已有的消球资格结算。
export function onRedOrbStarted(orbs: NanamiOrbComponent, actionId: string, instanceId: bigint): void {
    switch (actionId.toLowerCase()) {
        case "attack11":
            orbs.ApplyRedOrbResult(instanceId, 0, 0, 0, true, orbs.GetOrbState().bRedTriplePing);
            break;
        case "attack12": {
            const state = orbs.GetOrbState();
            if (!state.bRedFollowUp || state.RedStock < 1) {
                return;
            }

            // 普通二段的选球顺序仍待实测；这里读取训练夹具指定的组大小。
            const spentOrbs = state.bRedTriplePing ? 1 : Math.min(state.RedStock, orbs.GetTrainingRedGroupSize());
            const effectivePings = state.bRedTriplePing ? 3 : spentOrbs;
            const spentStars = Math.min(1, state.Stars);
            const keepFollowUp = state.Stars > spentStars;
            orbs.ApplyRedOrbResult(instanceId, spentOrbs, effectivePings, spentStars, keepFollowUp, false);
            break;
        }
    }
}
