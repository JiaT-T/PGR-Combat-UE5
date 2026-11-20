using UnrealBuildTool;

public class PGRCombat : ModuleRules
{
    // 声明战斗模块编译所需的引擎、输入和脚本依赖。
    public PGRCombat(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "EnhancedInput", "InputCore", "ScriptEnv", "PapiQuickjs" });
    }
}
