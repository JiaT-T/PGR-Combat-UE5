using UnrealBuildTool;

public class PGRCombatEditorTarget : TargetRules
{
    // 配置编辑器目标的构建规则并加载战斗模块。
    public PGRCombatEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("PGRCombat");
    }
}
