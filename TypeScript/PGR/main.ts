import { argv } from "puerts";
import { resolveNanamiAction } from "../Combat/Nanami/NanamiRules.js";

interface CombatScriptHost {
    ActionResolver: {
        // 注册供原生战斗组件调用的选招回调。
        Bind(callback: (combat: unknown, command: string, phase: string) => string): void;
    };
    // 通知原生宿主，脚本已完成选招规则的注册。
    ConfirmStartup(): void;
}

const host = argv.getByName("CombatHost") as CombatScriptHost | undefined;
if (!host) {
    throw new Error("Missing CombatHost startup argument");
}

// 将原生输入委托转发给七实的动作选择规则。
host.ActionResolver.Bind((_combat, command, phase) => resolveNanamiAction(command, phase));
host.ConfirmStartup();
