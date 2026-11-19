import { argv } from "puerts";

interface CombatScriptHost {
    GetStartupToken(): string;
    ConfirmStartup(reply: string): void;
}

const host = argv.getByName("CombatHost") as CombatScriptHost | undefined;
if (!host) {
    throw new Error("Missing CombatHost startup argument");
}

const token = host.GetStartupToken();
host.ConfirmStartup("PGR/" + token);
