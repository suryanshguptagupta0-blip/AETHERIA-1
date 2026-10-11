#!/usr/bin/env python3
"""Structural source checks; deliberately does not claim UHT/UE compilation."""
import json
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'Source/Aetheria'
errors = []
def require(condition, message):
    if not condition: errors.append(message)
project = json.loads((ROOT/'Aetheria.uproject').read_text())
require(project.get('EngineAssociation') == '5.6', 'Expected UE5.6 engine association')
require(any(m['Name'] == 'Aetheria' for m in project['Modules']), 'Missing native Aetheria module')
for name in ('Aetheria', 'AetheriaEditor', 'AetheriaServer'):
    require((ROOT/f'Source/{name}.Target.cs').is_file(), f'Missing {name} target')
for file in MODULE.rglob('*'):
    if file.suffix not in ('.cpp', '.h'): continue
    source = file.read_text()
    includes = re.findall(r'#include\s+"([^"]+)"', source)
    for include in includes:
        if include.endswith('.generated.h'): continue
        if 'Aetheria' in include:
            require((MODULE/include).exists() or (file.parent/include).exists(), f'{file.relative_to(ROOT)} missing include {include}')
    if 'GENERATED_BODY()' in source:
        generated = [i for i in includes if i.endswith('.generated.h')]
        require(generated == [file.stem+'.generated.h'], f'{file.name} generated header mismatch')
        require(bool(includes) and includes[-1].endswith('.generated.h'), f'{file.name} generated header must be last include')
for header in MODULE.rglob('*.h'):
    implementation = header.with_suffix('.cpp')
    text = header.read_text()
    for rpc in re.findall(r'UFUNCTION\((?:Server|Client),\s*Reliable\)\s+void\s+(\w+)', text):
        require(implementation.exists() and f'::{rpc}_Implementation(' in implementation.read_text(), f'Missing RPC implementation {rpc}')
engine = (ROOT/'Config/DefaultEngine.ini').read_text()
game = (ROOT/'Config/DefaultGame.ini').read_text()
native_mode = 'GlobalDefaultGameMode=/Script/Aetheria.AetheriaGameMode' in engine
imported_mode = 'GlobalDefaultGameMode=/Game/Core/BP_AetheriaGameMode.BP_AetheriaGameMode_C' in engine
require(native_mode or (imported_mode and (ROOT/'Content/Core/BP_AetheriaGameMode.uasset').is_file() and (ROOT/'Content/Characters/BP_AetheriaTraveler.uasset').is_file()), 'Default game mode must be native or the saved imported avatar game mode')
require('bAllowDevelopmentProfiles=False' in game, 'Development profile identities must default off')
require('bSupportsPortrait=False' in engine and 'Orientation=SensorLandscape' in engine, 'Landscape Android config missing')
require('r.DynamicGlobalIlluminationMethod=0' in engine and 'r.Nanite.ProjectEnabled=False' in engine, 'Mobile renderer defaults missing')
require('MaximumDistrictPlayers=8' in game, 'Eight-player district limit missing')
require('TargetSDKVersion=34' in engine, 'UE5.6 development APK targets SDK34; store/API upgrades require engine validation')
for file in ('Core/AetheriaPlayerController.cpp','Core/AetheriaPlayerState.cpp','Characters/AetheriaCharacter.cpp','World/AetheriaWorldRuntime.cpp','UI/AetheriaRootWidget.cpp','Rules/AetheriaRules.cpp'):
    require((MODULE/file).is_file(), f'Missing native implementation {file}')
result = {'check':'project_structure', 'passed':not errors, 'errors':errors, 'unreal_compile':'not_run', 'meaning':'Static source/config checks only; UHT and UE builds remain required.'}
print(json.dumps(result, indent=2))
raise SystemExit(1 if errors else 0)
