declare module "puerts" {
    export const argv: {
        // 按名称读取宿主启动脚本时传入的参数。
        getByName(name: string): unknown;
    };
}
