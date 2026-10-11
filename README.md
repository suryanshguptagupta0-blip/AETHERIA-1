# AETHERIA — FRACTURED REALITIES

## Open in Codespaces

[Open this repository in Codespaces](https://codespaces.new/suryanshguptagupta0-blip/AETHERIA-1?quickstart=1).

The dev container runs the portable rules and build/APK parser checks on creation. It does not download Unreal Engine or start a native APK build. The initial Codespace is a source-development workspace; the latest account inspection found no running Codespace, and the integration cannot manage machine selection (HTTP403).

Use `bash .devcontainer/check-source.sh` to repeat the source checks. For Android prerequisites, follow [BuildScripts/README.md](BuildScripts/README.md). A real accessible UE5.6.1 Linux engine/build host is still required. No APK is supplied.


UE5.6 native source milestone v0.1.1, developed from the supplied master PDF. **This is not a verified Unreal executable or Android APK.** The separate [browser prototype](https://aetheria-magic-world.spiffymaple32.chatgpt.site) is playable and has been updated to v1.4.0 with typed NPC conversations.

## What is in this project

- `.uproject`, native Game/Editor/Server targets, C++ gameplay module, configuration and build/verification scripts.
- Server-owned player profile, GUID save slots, permanent initial random awakening at Basic, eleven element definitions and six independent mastery stages.
- Chapter rules for hospital doctor, Awakening Hall/Seraphine/crystal, target/dodge/element trial, two-phase Guardian, seven facility visits and permanent Seal/Study choice.
- Native third-person character/camera, Enhanced Input, predicted sprint flag, guarded cast/dodge intents, approved-hit projectiles, replicated Guardian telegraphs and emergency hospital recovery.
- A runtime engineering district with hospital, hall, academy, guild, houses, market, bank, inn and library. Entrances and room furniture use engine basic shapes; this is temporary geometry.
- UMG menu, male/female identity choice, HUD, city map/player marker, typed bounded NPC chat, separate service buttons, settings and touch movement/look. NPC text is authored, contextual and remembers the player through saved conversations; no paid AI endpoint is used.
- Bounded spell experimentation/stabilization/reproduction/registration, Advanced-parent fusion, currency conservation and validated versioned serialization in the portable Rules module.
- Verified MIT male/female rigged FBX source meshes and six idle/walk/run FBX sources, plus the OFL Hindi font and all notices/manifests. Assets are downloaded, not imported or tested in UE. The Editor helper now creates the avatar pawn and GameMode Blueprints, activates them and adds cook roots only after successful import/save. Android packaging invokes this real Editor step; the helper is still unexecuted here.
- Free official Android SDK34, NDK r27c, build-tools35.0.1, CMake3.22.1 and Temurin JDK21 installed and probed in the development workspace. Reproducible bootstrap scripts verify official archive checksums. Toolchain setup does not include Unreal Engine.

## Run the available checks

```sh
python3 Tests/validate_project.py
mkdir -p Tests/.build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti Source/Aetheria/Rules/AetheriaRules.cpp Tests/RulesTests.cpp -I Source/Aetheria/Rules -o Tests/.build/rules-tests
Tests/.build/rules-tests
```

The strict rules executable passed **1,991 assertions, zero failures**, including the sanitizer run. Tests/results.json records commands and exact source hashes. The same rules source now compiles to an ELF64 AArch64 object with the actual Android NDK compiler and strict warnings; Tests/android-rules-results.json records that evidence. It was not executed on Android. The build/APK parser suite passes 38 regression tests. Structural source/config checks and Python script syntax checks pass; these checks do not compile Unreal code.

Actual engine commands are in [BuildScripts/README.md](BuildScripts/README.md). Set UE_ROOT to a real UE5.6 Linux installation; the Android toolchain gate requires 5.6.1 or a later 5.6 patch and the installed engine's Turnkey confirmation. Dedicated Server needs a source-built engine. The launcher calls real UBT/UAT and checks target-specific receipts and binary products. The APK verifier checks real signatures, package identity, landscape on the actual GameActivity and ARM64 shared-library headers. No placeholder APK is supplied.

## Remaining release work

The Android SDK/NDK/JDK dependency is now resolved. Unreal Engine is still absent; the connected GitHub account received HTTP404 for EpicGames/UnrealEngine and there is no configured remote build host. Official Linux engine download/source access requires Epic account setup. A physical device and dedicated multiplayer test run are also unavailable. UHT/native compilation, cooked chapter playthrough, asset import/retarget/materials, Hindi shaping, signed APK/device install and 4–8-client QA have **not** passed. Server-local saves and development GUIDs are not production authentication or cross-server storage.

The native UI exposes initial awakening only. Later-element awakening, native spell/fusion editing UI, distinct native implementations of every spell form, full customization, real walking NPC schedules, final realistic art and production animation graphs remain unfinished. Current attacks use the guarded basic projectile adapter; rules support does not prove all native effects are implemented. The mesh adapter uses simple loops until a validated Animation Blueprint replaces it. Native performance has not been measured.

See Documentation/Milestones/Acceptance.md, Documentation/Milestones/Verification.md and the art/performance notes for precise evidence and remaining gates. Future iterations must resolve actual engine/compiler/device failures before claiming a native playable release. No unattended continuous-update service or paid resource was installed.
