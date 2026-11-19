using UnrealBuildTool;

public class PGRCombat : ModuleRules
{
    public PGRCombat(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "EnhancedInput", "InputCore", "ScriptEnv", "PapiQuickjs" });
    }
}
