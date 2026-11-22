const swordShieldCombo = ["Attack01", "Attack02", "Attack03", "Attack04"];
const shieldAxeCombo = ["Attack61", "Attack62", "Attack63", "Attack64"];

// 空闲时从当前形态的首段起手，同一套普攻依次续段，第四段自然收招。
export function resolveBasicAttack(phase: string, activeActionId: string, shieldAxe: boolean): string {
    if (phase !== "Pressed") {
        return "";
    }

    // Puerts 以字符串传入 FName，匹配时沿用 UE 忽略大小写的规则。
    const combo = shieldAxe ? shieldAxeCombo : swordShieldCombo;
    const active = activeActionId.toLowerCase();
    if (active === "none") {
        return combo[0];
    }
    const index = combo.findIndex(actionId => actionId.toLowerCase() === active);
    return index >= 0 ? combo[index + 1] ?? "" : "";
}
