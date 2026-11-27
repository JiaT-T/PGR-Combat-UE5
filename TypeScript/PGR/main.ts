import { argv } from "puerts";
import { CombatComponent, resolveNanamiAction } from "../Combat/Nanami/NanamiRules.js";
import { NanamiOrbComponent, onOrbStarted } from "../Combat/Nanami/SignalOrb.js";

interface CombatScriptHost {
    ActionResolver: {
        // 注册供原生战斗组件调用的选招回调。
        Bind(callback: (combat: CombatComponent, command: string, phase: string, directional: boolean) => string): void;
    };
    NanamiOrbActionStarted: {
        // 注册七实动作确认起播后的资源结算。
        Bind(callback: (orbs: NanamiOrbComponent, actionId: string, instanceId: bigint) => void): void;
    };
    // 通知原生宿主，脚本已完成选招规则的注册。
    ConfirmStartup(): void;
}

const host = argv.getByName("CombatHost") as CombatScriptHost | undefined;
if (!host) {
    throw new Error("Missing CombatHost startup argument");
}

// 将原生输入委托转发给七实的动作选择规则。
host.ActionResolver.Bind(resolveNanamiAction);
host.NanamiOrbActionStarted.Bind(onOrbStarted);
host.ConfirmStartup();
