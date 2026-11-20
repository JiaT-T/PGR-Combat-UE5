import { ENanamiOrbColor } from "ue";

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
    GetTrainingGroupSize(color: ENanamiOrbColor): number;
    // 将本次动作的实际支出、有效消球档位和下一状态一起提交。
    ApplyOrbResult(instanceId: bigint, color: ENanamiOrbColor, spentOrbs: number, effectivePings: number, spentStars: number,
                      followUp: boolean, triplePing: boolean): void;
}

const redOrb = {
    color: ENanamiOrbColor.Red,
    opening: "Attack11",
    followUp: "Attack12",
    stockKey: "RedStock",
    followUpKey: "bRedFollowUp",
    triplePingKey: "bRedTriplePing",
} as const;

const blueOrb = {
    color: ENanamiOrbColor.Blue,
    opening: "Attack21",
    followUp: "Attack22",
    stockKey: "BlueStock",
    followUpKey: "bBlueFollowUp",
    triplePingKey: "bBlueTriplePing",
} as const;

const signalOrbs = [redOrb, blueOrb] as const;

// 按输入颜色及该色独立的二段资格选招。库存为零时连一段也不能发动。
export function resolveSignalOrb(state: OrbState | undefined, color: ENanamiOrbColor): string {
    const orb = signalOrbs.find(candidate => candidate.color === color);
    if (!orb || !state || state[orb.stockKey] < 1) {
        return "";
    }
    return state[orb.followUpKey] ? orb.followUp : orb.opening;
}

// 两种颜色分别按各自的一、二段动作匹配，其他动作跳过信号球结算。
export function onOrbStarted(orbs: NanamiOrbComponent, actionId: string, instanceId: bigint): void {
    const action = actionId.toLowerCase();
    const orb = signalOrbs.find(candidate =>
        candidate.opening.toLowerCase() === action || candidate.followUp.toLowerCase() === action);
    if (!orb) {
        return;
    }

    const state = orbs.GetOrbState();
    const triple = state[orb.triplePingKey];
    if (action === orb.opening.toLowerCase()) {
        orbs.ApplyOrbResult(instanceId, orb.color, 0, 0, 0, true, triple);
        return;
    }

    const stock = state[orb.stockKey];
    if (!state[orb.followUpKey] || stock < 1) {
        return;
    }

    const spent = triple ? 1 : Math.min(stock, orbs.GetTrainingGroupSize(orb.color));
    const spentStars = Math.min(1, state.Stars);
    // GP 中零星且耗光本色库存时，图标又显示约三秒；零库存始终拒绝输入。
    const keepIcon = state.Stars > spentStars || (spentStars === 0 && spent === stock);
    orbs.ApplyOrbResult(instanceId, orb.color, spent, triple ? 3 : spent, spentStars, keepIcon, false);
}
