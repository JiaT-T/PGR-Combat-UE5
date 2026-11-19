import { argv } from "puerts";
import { resolveNanamiAction } from "../Combat/Nanami/NanamiRules.js";

interface CombatScriptHost {
    ActionResolver: {
        Bind(callback: (combat: unknown, command: string, phase: string) => string): void;
    };
    GetStartupToken(): string;
    ConfirmStartup(reply: string): void;
}

const host = argv.getByName("CombatHost") as CombatScriptHost | undefined;
if (!host) {
    throw new Error("Missing CombatHost startup argument");
}

host.ActionResolver.Bind((_combat, command, phase) => resolveNanamiAction(command, phase));
const token = host.GetStartupToken();
host.ConfirmStartup("PGR/" + token);
