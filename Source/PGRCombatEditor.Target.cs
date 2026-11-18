using UnrealBuildTool;

public class PGRCombatEditorTarget : TargetRules
{
    public PGRCombatEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("PGRCombat");
    }
}
