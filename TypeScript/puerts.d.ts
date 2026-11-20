declare module "puerts" {
    export const argv: {
        // 按名称读取宿主启动脚本时传入的参数。
        getByName(name: string): unknown;
    };
}

declare module "ue" {
    export const NanamiOrbComponent: {
        // 获取 UE 注册的组件类型以查询当前角色的实例。
        StaticClass(): unknown;
    };
}
