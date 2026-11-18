using UnrealBuildTool;

public class PGRCombat : ModuleRules
{
    public PGRCombat(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("Core");
    }
}
