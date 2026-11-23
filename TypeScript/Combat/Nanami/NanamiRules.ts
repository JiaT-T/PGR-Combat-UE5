import { resolveBasicAttack } from "./BasicAttack.js";
import { ENanamiOrbColor, ENanamiWeaponForm, NanamiFormComponent, NanamiOrbComponent as OrbComponentClass } from "ue";
import { NanamiOrbComponent, resolveSignalOrb } from "./SignalOrb.js";
import { NanamiForm, resolveYellowOrb } from "./YellowOrb.js";

export interface CombatComponent {
    // 读取角色此刻正在播放的动作，空闲时 UE 返回 None。
    GetActiveActionId(): string;
    // 定位发出输入的角色，只在该角色上查找专属组件。
    GetOwner(): {
        // 通过 UE 类型定位七实的信号球组件。
        GetComponentByClass<T>(componentClass: unknown): T | undefined;
    };
}

// 按输入指令分派七实的动作规则，未知指令返回空动作。
export function resolveNanamiAction(combat: CombatComponent, command: string, phase: string): string {
    const form = combat.GetOwner().GetComponentByClass<NanamiForm>(NanamiFormComponent.StaticClass());
    if (command === "BasicAttack" || command === "YellowOrb") {
        if (command === "YellowOrb") {
            return form ? resolveYellowOrb(form, phase) : "";
        }
        const shieldAxe = form?.GetWeaponForm() === ENanamiWeaponForm.ShieldAxe;
        return resolveBasicAttack(phase, combat.GetActiveActionId(), shieldAxe);
    }

    if ((command === "RedOrb" || command === "BlueOrb") && phase === "Pressed") {
        if (!form?.CanUseRedBlueOrbs()) {
            return "";
        }
        const orbs = combat.GetOwner().GetComponentByClass<NanamiOrbComponent>(OrbComponentClass.StaticClass());
        const color = command === "RedOrb" ? ENanamiOrbColor.Red : ENanamiOrbColor.Blue;
        return resolveSignalOrb(orbs?.GetOrbState(), color);
    }

    return "";
}
