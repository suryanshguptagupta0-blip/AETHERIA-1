# Build and verification

This launcher supports a Linux x86_64 build host. It calls real Unreal tools and fails missing prerequisites or outputs. No Unreal executable or APK has been built in this workspace.

## Source checks

```sh
python3 Tests/validate_project.py
python3 -m unittest discover -s Tests -p 'test_*.py' -v
mkdir -p Tests/.build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti Source/Aetheria/Rules/AetheriaRules.cpp Tests/RulesTests.cpp -I Source/Aetheria/Rules -o Tests/.build/rules-tests
Tests/.build/rules-tests
```

The portable rules passed 1,991 assertions and the tooling parsers passed 38 tests. Synthetic fixtures test rejection logic; they are not engine binaries or APK evidence. Structural checks are not UHT compilation.

## Free Android prerequisites

The workspace now has SDK 34, NDK r27c (27.2.12479018), build-tools 35.0.1, CMake 3.22.1 and Temurin JDK 21 under `/workspace/toolchains`. These are installed build tools, not included in the source ZIP. The bootstrap scripts use official Google/Adoptium downloads, verify checksums and preserve the upstream licenses. A Java runtime alone is insufficient; `javac` must be present.

On another Linux build host, choose writable toolchain paths and run:

```sh
python3 BuildScripts/setup_java.py --dest /path/to/toolchains/java21
export JAVA_HOME=/path/to/toolchains/java21
python3 BuildScripts/setup_android.py --sdk /path/to/toolchains/android-sdk --accept-licenses
export ANDROID_HOME=/path/to/toolchains/android-sdk
python3 BuildScripts/build.py toolchain
```

`--accept-licenses` applies to the listed Android package license prompts. The SDK bootstrap pins command-line-tools 13.0. It does not install Unreal. The toolchain command executes the actual Java compiler/runtime, NDK compiler, adb, aapt2, apksigner and CMake; without an engine it reports Unreal compatibility as not run.

## Required engine access

[Official Linux Unreal downloads](https://www.unrealengine.com/en-US/linux) require an Epic account. [Official source access](https://www.unrealengine.com/en-US/ue-on-github) requires linking a personal GitHub account to Epic and accepting the EpicGames invitation/EULA. The currently connected GitHub account received 404 for EpicGames/UnrealEngine. No configured remote build host or cloud identity is present. No paid resources or unofficial engine mirrors were used.

Use a real UE5.6 Linux engine. The prepared Android toolchain is pinned for UE5.6.1 or a later 5.6 patch; the launcher rejects 5.6.0 for Android. Check the installed engine's `Engine/Extras/Android/SetupAndroid.sh` and run its real Turnkey verification. The versioned Epic Android web page mixes 5.6 history with a current 5.7 section, so it is not sufficient evidence by itself. [Epic's version history](https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine?application_version=5.6&lang=en-US) lists SDK 34 and NDK r27c for 5.6. The launcher retains the actual Turnkey log and requires Android `Status=Valid`.

```sh
export UE_ROOT=/path/to/UnrealEngine
python3 BuildScripts/build.py check
python3 BuildScripts/build.py toolchain
python3 BuildScripts/build.py editor
python3 BuildScripts/build.py game
python3 BuildScripts/build.py server
```

The Server target requires a source-built engine with dedicated-server support. After successful UBT, each target must have its own matching `.target` receipt and existing ELF build products; an arbitrary file left in Binaries cannot satisfy the gate. Up-to-date legitimate UBT output is permitted. These receipts still require validation with the actual engine.

## Avatar preparation and Android packaging

```sh
python3 BuildScripts/build.py avatars
python3 BuildScripts/build.py android --output /path/to/Aetheria-Android
```

`avatars` builds the real Editor target and runs `UnrealEditor-Cmd` with the Python import helper. It imports/reimports the two licensed meshes and six animations, saves/configures the Traveler and GameMode Blueprints, activates that GameMode and adds cook roots. The launcher checks a new Editor-written receipt and all ten saved packages. This step updates the owned imported source-art assets; preserve manual edits separately before reimporting. Rig pose, scale, axes, materials, animation blending and Hindi shaping still need an actual Editor/gameplay review. The importer has not run in this workspace.

`android` probes the tools, requires actual engine Turnkey approval, prepares avatars and calls real UAT BuildCookRun with ASTC and the Entry map. It requires a newly generated APK in the selected output and then runs real apksigner/aapt2 plus the shared-library checks. Landscape must be on UE5's GameActivity; a landscape splash activity cannot pass. The package must contain real AArch64 `libUnreal.so`. This is a development APK targeting API 34; future store/API requirements need a separately validated engine/toolchain upgrade.

For the Android rules compile already completed here:

```sh
mkdir -p Tests/.build/android
"$ANDROID_HOME/ndk/27.2.12479018/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++" -std=c++17 -Os -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti -fPIC -c Source/Aetheria/Rules/AetheriaRules.cpp -o Tests/.build/android/AetheriaRules.o
```

That output is a relocatable rules object, not an executable, UE module, APK or on-device run.

## Gameplay and multiplayer gates

Open Aetheria.uproject in the actual Editor and play the Entry map; its game mode generates the engineering district. Play the whole hospital → awakening → trial → two-phase Guardian → seven facilities → final mentor decision. A LAN source-development server runs the compiled AetheriaServer with `/Engine/Maps/Entry -log -port=7777`; clients use `open server-ip:7777`. Development reconnect IDs are off by default and are not authentication.

Resolve every UHT/compiler/cook failure in the engine before a native release claim. Signed APK verification does not prove installation, touch controls, frame timing, Hindi rendering or multiplayer. A physical-device pass, 4–8-client QA, production identity/storage and a production signing workflow are still required.
