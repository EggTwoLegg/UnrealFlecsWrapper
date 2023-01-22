using UnrealBuildTool;

public class FlecsLibrary : ModuleRules
{
	public FlecsLibrary (ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"NavigationSystem"
		});

		
		PublicIncludePaths.AddRange(new string[] {"FlecsLibrary/Public"});
		PrivateIncludePaths.AddRange(new string[] {"FlecsLibrary/Private"});
		CppStandard = CppStandardVersion.Cpp20;
	}
}
