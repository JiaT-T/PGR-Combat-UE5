const swordShieldCombo = ["Attack01", "Attack02", "Attack03", "Attack04"];
const shieldAxeCombo = ["Attack61", "Attack62", "Attack63", "Attack64"];

// 盾斧长按发动重击，点按按形态循环四段；其他动作只在配置了普攻衔接时返回首段。
export function resolveBasicAttack(phase: string, activeActionId: string, shieldAxe: boolean, hasBasicFollowUp: boolean): string {
    const active = activeActionId.toLowerCase();
    // 两种必杀收势后都回剑盾；同帧的形态通知和输入先后顺序不影响接招。
    if (active === "attack51" || active === "attack52") {
        return phase === "Pressed" && hasBasicFollowUp ? swordShieldCombo[0] : "";
    }
    if (phase === "Held") {
        return shieldAxe && active !== "yellowhold" && active !== "attack99" ? "Attack99" : "";
    }
    if (phase !== "Pressed") {
        return "";
    }

    // Puerts 以字符串传入 FName，匹配时沿用 UE 忽略大小写的规则。
    const combo = shieldAxe ? shieldAxeCombo : swordShieldCombo;
    if (active === "none") {
        return combo[0];
    }
    const index = combo.findIndex(actionId => actionId.toLowerCase() === active);
    if (index >= 0) {
        return combo[(index + 1) % combo.length];
    }
    return hasBasicFollowUp ? combo[0] : "";
}
