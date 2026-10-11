# Release gates

| Gate | Current evidence | Remaining acceptance |
| --- | --- | --- |
| Native rules | Strict portable C++ executable and sanitizer checks pass | Maintain tests when gameplay rules change |
| UE build | .uproject and Game/Editor/Server source targets | UnrealHeaderTool, Editor/Game/Server actual compilation with UE5.6 |
| Chapter one | Native rules and gameplay adapter source | Play hospital → awakening → trial → both Guardian phases → seven city visits → final mentor choice in cooked UE build |
| Persistence | Versioned bounded server rules serializer; SaveGame adapter | UE restart/reconnect runs, failure recovery, production transactional storage |
| Android | Official SDK/NDK/JDK installed; actual ARM64 rules object compiles; 38 tooling regression tests pass | Installed Unreal Turnkey compatibility, real signed ARM64 APK, installation, touch QA and measured device captures |
| Multiplayer | Server RPC/replication source, eight-player limit | 4–8 clients on one dedicated server, latency/loss/reconnect and hostile intent tests |
| Realistic art | Free source audit; engineering geometry explicitly labeled | Licensed rigged male/female meshes, animation graph, final buildings/materials and performance budgets |
| Localization | Native UI translation table and authored NPC text | Hindi font/shaping, all dialogue coverage, landscape device QA |
| Spell systems | Experiment/stabilize/reproduce/register/fusion rule pipeline | Native editor UI and distinct native behaviors for all supported spell forms |

Passing the portable executable does not close the Unreal, Android, art or multiplayer gates. The existing hosted browser prototype remains separately playable and is not an APK or native build.

Continuous improvements mean reviewable iterations and rerun acceptance checks during development. No unattended recurring update service is installed.
