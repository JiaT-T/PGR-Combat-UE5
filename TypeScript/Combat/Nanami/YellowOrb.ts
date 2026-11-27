import { ENanamiWeaponForm } from "ue";

export interface NanamiForm {
    // 读取已生效的剑盾或盾斧形态。
    GetWeaponForm(): ENanamiWeaponForm;
    // 查询当前形态是否允许发动红球和蓝球。
    CanUseRedBlueOrbs(): boolean;
    // 查询点黄切盾斧所需的星芒条件是否成立。
    HasStarlightForSwordToAxe(): boolean;
    // 查询本次重新按住是否达到发起焚冲的条件。
    CanStartYellowHold(): boolean;
}

// 松手选切形技，长按成立选一次完整焚冲；选招不修改武器或资源。
export function resolveYellowOrb(form: NanamiForm, phase: string): string {
    if (phase === "Held") {
        return form.CanStartYellowHold() ? "YellowHold" : "";
    }
    if (phase === "Released") {
        switch (form.GetWeaponForm()) {
            case ENanamiWeaponForm.SwordShield:
                return form.HasStarlightForSwordToAxe() ? "YellowSwordToAxe" : "";
            case ENanamiWeaponForm.ShieldAxe:
                return "YellowAxeToSword";
        }
    }
    return "";
}
