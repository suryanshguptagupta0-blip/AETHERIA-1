using UnrealBuildTool;
using System.Collections.Generic;
public class AetheriaTarget : TargetRules {
 public AetheriaTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Game;
  DefaultBuildSettings = BuildSettingsVersion.V5;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
  ExtraModuleNames.Add("Aetheria");
 }
}
