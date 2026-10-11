using UnrealBuildTool;
using System.Collections.Generic;
public class AetheriaServerTarget : TargetRules {
 public AetheriaServerTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Server;
  DefaultBuildSettings = BuildSettingsVersion.V5;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
  ExtraModuleNames.Add("Aetheria");
 }
}
