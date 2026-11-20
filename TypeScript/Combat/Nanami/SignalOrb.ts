export interface OrbState {
    readonly RedStock: number;
    readonly BlueStock: number;
    readonly Stars: number;
    readonly bBlueFollowUp: boolean;
    readonly bBlueTriplePing: boolean;
    readonly bRedFollowUp: boolean;
    readonly bRedTriplePing: boolean;
}

export interface NanamiOrbComponent {
    // 读取当前角色信号球数据的一份快照。
    GetOrbState(): OrbState;
    // 读取训练场样例中普通二段所选同色组的大小。
    GetTrainingGroupSize(blue: boolean): number;
    // 将本次动作的实际支出、有效消球档位和下一状态一起提交。
    ApplyOrbResult(instanceId: bigint, blue: boolean, spentOrbs: number, effectivePings: number, spentStars: number,
                      followUp: boolean, triplePing: boolean): void;
}

// 按输入颜色及该色独立的二段资格选招。库存为零时连一段也不能发动。
export function resolveSignalOrb(state: OrbState | undefined, blue: boolean): string {
    if (!state || (blue ? state.BlueStock : state.RedStock) === 0) {
        return "";
    }
    if (blue) {
        return state.bBlueFollowUp ? "Attack22" : "Attack21";
    }
    return state.bRedFollowUp ? "Attack12" : "Attack11";
}

// 只结算已确认起播的红蓝技能，普攻与其他动作不改变球态。
export function onOrbStarted(orbs: NanamiOrbComponent, actionId: string, instanceId: bigint): void {
    const action = actionId.toLowerCase();
    const blue = action === "attack21" || action === "attack22";
    if (!blue && action !== "attack11" && action !== "attack12") {
        return;
    }

    const state = orbs.GetOrbState();
    const triple = blue ? state.bBlueTriplePing : state.bRedTriplePing;
    if (action === "attack11" || action === "attack21") {
        orbs.ApplyOrbResult(instanceId, blue, 0, 0, 0, true, triple);
        return;
    }

    const stock = blue ? state.BlueStock : state.RedStock;
    const followUp = blue ? state.bBlueFollowUp : state.bRedFollowUp;
    if (!followUp || stock < 1) {
        return;
    }

    const spent = triple ? 1 : Math.min(stock, orbs.GetTrainingGroupSize(blue));
    const spentStars = Math.min(1, state.Stars);
    // GP 中零星且耗光本色库存时，图标又显示约三秒；零库存始终拒绝输入。
    const keepIcon = state.Stars > spentStars || (spentStars === 0 && spent === stock);
    orbs.ApplyOrbResult(instanceId, blue, spent, triple ? 3 : spent, spentStars, keepIcon, false);
}
