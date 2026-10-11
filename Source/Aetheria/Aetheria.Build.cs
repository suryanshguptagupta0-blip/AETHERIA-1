using UnrealBuildTool;
public class Aetheria : ModuleRules {
 public Aetheria(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  CppStandard = CppStandardVersion.Cpp20;
  PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "GameplayTags", "AIModule", "NavigationSystem", "NetCore", "Json", "JsonUtilities" });
  PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
 }
}
