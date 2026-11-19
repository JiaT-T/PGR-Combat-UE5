// 普攻按下时选择剑盾首段，松开时返回空动作。
export function resolveBasicAttack(phase: string): string {
    return phase === "Pressed" ? "Attack01" : "";
}
