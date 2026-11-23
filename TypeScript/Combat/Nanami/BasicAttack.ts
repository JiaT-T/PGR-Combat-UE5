const swordShieldCombo = ["Attack01", "Attack02", "Attack03", "Attack04"];
const shieldAxeCombo = ["Attack61", "Attack62", "Attack63", "Attack64"];

// 盾斧可长按发动重击；点按按形态续段，盾斧第四段开窗后可回到第一段。
export function resolveBasicAttack(phase: string, activeActionId: string, shieldAxe: boolean): string {
    const active = activeActionId.toLowerCase();
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
    if (shieldAxe && index === combo.length - 1) {
        return combo[0];
    }
    return index >= 0 ? combo[index + 1] ?? "" : "";
}
