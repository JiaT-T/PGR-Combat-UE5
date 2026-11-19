import { argv } from "puerts";
import { resolveNanamiAction } from "../Combat/Nanami/NanamiRules.js";

interface CombatScriptHost {
    ActionResolver: {
        // 注册供原生战斗组件调用的选招回调。
        Bind(callback: (combat: unknown, command: string, phase: string) => string): void;
    };
    // 读取本次原生启动生成的校验令牌。
    GetStartupToken(): string;
    // 向原生宿主回传校验结果以确认脚本已启动。
    ConfirmStartup(reply: string): void;
}

const host = argv.getByName("CombatHost") as CombatScriptHost | undefined;
if (!host) {
    throw new Error("Missing CombatHost startup argument");
}

// 将原生输入委托转发给七实的动作选择规则。
host.ActionResolver.Bind((_combat, command, phase) => resolveNanamiAction(command, phase));
const token = host.GetStartupToken();
host.ConfirmStartup("PGR/" + token);
