using UnrealBuildTool;

public class PGRCombatTarget : TargetRules
{
    // 配置独立游戏目标的构建规则并加载战斗模块。
    public PGRCombatTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("PGRCombat");
    }
}
