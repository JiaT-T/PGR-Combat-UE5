import { resolveBasicAttack } from "./BasicAttack.js";

export function resolveNanamiAction(command: string, phase: string): string {
    if (command === "BasicAttack") {
        return resolveBasicAttack(phase);
    }

    return "";
}
