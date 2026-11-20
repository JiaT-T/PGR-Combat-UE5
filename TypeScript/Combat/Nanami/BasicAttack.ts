// 空闲时起手剑盾普攻，连按依次选择下一段，第四段自然收招。
export function resolveBasicAttack(phase: string, activeActionId: string): string {
    if (phase !== "Pressed") {
        return "";
    }

    // Puerts 以字符串传入 FName，匹配时沿用 UE 忽略大小写的规则。
    switch (activeActionId.toLowerCase()) {
        case "none":
            return "Attack01";
        case "attack01":
            return "Attack02";
        case "attack02":
            return "Attack03";
        case "attack03":
            return "Attack04";
        default:
            return "";
    }
}
