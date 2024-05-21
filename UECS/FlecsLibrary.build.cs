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
			"NavigationSystem",
			"AIModule", 
			"Navmesh"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"AIModule", 
			"Navmesh",
		});

		
		PublicIncludePaths.AddRange(new string[] {"FlecsLibrary/Public"});
		PrivateIncludePaths.AddRange(new string[] {"FlecsLibrary/Private"});
		PublicAdditionalLibraries.Add("C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64/Kernel32.lib");
		CppStandard = CppStandardVersion.Cpp20;
	}
}
