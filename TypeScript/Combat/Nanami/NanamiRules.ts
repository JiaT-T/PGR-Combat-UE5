import { resolveBasicAttack } from "./BasicAttack.js";

// 按输入指令分派七实的动作规则，未知指令返回空动作。
export function resolveNanamiAction(command: string, phase: string, activeActionId: string): string {
    if (command === "BasicAttack") {
        return resolveBasicAttack(phase, activeActionId);
    }

    return "";
}
