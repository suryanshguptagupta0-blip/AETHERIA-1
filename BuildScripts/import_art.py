"""UE5.6 Editor-only import and activation. Requires a real compiled editor.

Creates the licensed avatars and configures their pawn/game-mode cook roots.
This helper is unexecuted without Unreal; its existence is not import evidence.
Inspect imported pose, scale, materials and animations before any release.
"""
from pathlib import Path
import hashlib
import json
import re
import unreal
project = Path(unreal.Paths.project_dir()).resolve()
tools = unreal.AssetToolsHelpers.get_asset_tools()
sources = {}
def import_fbx(file, destination, name, skeleton=None):
    if not file.is_file(): raise RuntimeError(f'Missing licensed source file: {file}')
    sources[str(file.relative_to(project))] = hashlib.sha256(file.read_bytes()).hexdigest()
    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', skeleton is None)
    options.set_editor_property('import_as_skeletal', True)
    options.set_editor_property('import_animations', skeleton is not None)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION if skeleton else unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton: options.set_editor_property('skeleton', skeleton)
    task = unreal.AssetImportTask()
    for property_name, value in {'filename':str(file),'destination_path':destination,'destination_name':name,'automated':True,'save':True,'replace_existing':True,'options':options}.items(): task.set_editor_property(property_name,value)
    tools.import_asset_tasks([task])
    asset = unreal.EditorAssetLibrary.load_asset(destination+'/'+name)
    if not asset: raise RuntimeError(f'Import failed: {destination}/{name}. Inspect UE importer logs.')
    return asset
meshes, animations = {}, {}
for sex, prefix in [('Female','f'),('Male','m')]:
    meshes[sex] = import_fbx(project/f'Content/Characters/SourceArt/{sex}_Adult_01.fbx','/Game/Characters/Imported',f'SK_Rocketbox{sex}')
    if not isinstance(meshes[sex],unreal.SkeletalMesh): raise RuntimeError('Expected a skeletal mesh; inspect FBX import settings')
    skeleton = meshes[sex].get_editor_property('skeleton')
    for label, file in [('Idle',f'{prefix}_idle_breathe_01.max.fbx'),('Walk',f'{prefix}_walk_neutral.max.fbx'),('Run',f'{prefix}_run_neutral.max.fbx')]:
        animations[sex,label] = import_fbx(project/f'Content/Animations/SourceArt/{file}','/Game/Animations/Imported',f'A_{sex}{label}',skeleton)
        if not isinstance(animations[sex,label],unreal.AnimSequence): raise RuntimeError('Expected an animation sequence')
        if animations[sex,label].get_editor_property('skeleton') != skeleton: raise RuntimeError(f'{sex} {label} has a different skeleton')
path = '/Game/Characters/BP_AetheriaTraveler'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    factory = unreal.BlueprintFactory(); factory.set_editor_property('parent_class',unreal.AetheriaCharacter)
    if not tools.create_asset('BP_AetheriaTraveler','/Game/Characters',unreal.Blueprint,factory): raise RuntimeError('Could not create traveler Blueprint')
blueprint = unreal.EditorAssetLibrary.load_asset(path)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
character_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
if not character_class: raise RuntimeError('Blueprint class creation failed')
defaults = unreal.get_default_object(character_class)
for sex in ('Male','Female'):
    defaults.set_editor_property(sex.lower()+'_mesh',meshes[sex])
    for label in ('Idle','Walk','Run'): defaults.set_editor_property(sex.lower()+'_'+label.lower(),animations[sex,label])
if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False): raise RuntimeError('Could not save traveler Blueprint')
mode_path = '/Game/Core/BP_AetheriaGameMode'
if not unreal.EditorAssetLibrary.does_asset_exist(mode_path):
    factory = unreal.BlueprintFactory(); factory.set_editor_property('parent_class',unreal.AetheriaGameMode)
    if not tools.create_asset('BP_AetheriaGameMode','/Game/Core',unreal.Blueprint,factory): raise RuntimeError('Could not create game-mode Blueprint')
mode_blueprint = unreal.EditorAssetLibrary.load_asset(mode_path)
unreal.BlueprintEditorLibrary.compile_blueprint(mode_blueprint)
mode_class = unreal.EditorAssetLibrary.load_blueprint_class(mode_path)
if not mode_class: raise RuntimeError('Game-mode Blueprint compile failed')
unreal.get_default_object(mode_class).set_editor_property('default_pawn_class',character_class)
if not unreal.EditorAssetLibrary.save_asset(mode_path, only_if_is_dirty=False): raise RuntimeError('Could not save game-mode Blueprint')
if not unreal.EditorAssetLibrary.save_directory('/Game/Characters', only_if_is_dirty=False, recursive=True): raise RuntimeError('Character assets did not save')
if not unreal.EditorAssetLibrary.save_directory('/Game/Animations', only_if_is_dirty=False, recursive=True): raise RuntimeError('Animation assets did not save')

# Activate only after both Blueprints and imported assets are saved successfully.
# Keep all unrelated config and repeated UE array keys intact.
engine_file = project/'Config/DefaultEngine.ini'
engine_text = engine_file.read_text()
mode_reference = mode_path+'.BP_AetheriaGameMode_C'
for key in ('GlobalDefaultGameMode','GlobalDefaultServerGameMode'):
    pattern = rf'(?m)^{key}=.*$'
    if len(re.findall(pattern,engine_text)) != 1: raise RuntimeError(f'Expected one {key} config entry')
    engine_text = re.sub(pattern,key+'='+mode_reference,engine_text)
game_file = project/'Config/DefaultGame.ini'
game_text = game_file.read_text()
for directory in ('/Game/Core','/Game/Characters','/Game/Animations'):
    line = f'+DirectoriesToAlwaysCook=(Path="{directory}")'
    if line not in game_text:
        if '[/Script/UnrealEd.ProjectPackagingSettings]' not in game_text: raise RuntimeError('Packaging config section missing')
        game_text = game_text.replace('[/Script/UnrealEd.ProjectPackagingSettings]', '[/Script/UnrealEd.ProjectPackagingSettings]\n'+line,1)
engine_file.write_text(engine_text)
game_file.write_text(game_text)
receipt_dir = project/'Saved/BuildEvidence'
receipt_dir.mkdir(parents=True,exist_ok=True)
asset_list = list(meshes.values()) + list(animations.values()) + [blueprint, mode_blueprint]
assets = []
for asset in asset_list:
    object_path = asset.get_path_name()
    package = object_path.split('.')[0]
    package_file = 'Content/' + package.removeprefix('/Game/') + '.uasset'
    if not (project/package_file).is_file(): raise RuntimeError(f'Saved package missing: {package_file}')
    assets.append({'object_path':object_path,'package_file':package_file})
engine_version = json.loads((Path(unreal.Paths.engine_dir())/'Build/Build.version').read_text())
receipt = {'schema_version':1,'status':'imported_and_saved','project':str(project),'engine_version':engine_version,'assets':assets,'game_mode':mode_reference,'pawn':path+'.BP_AetheriaTraveler_C','engine':unreal.SystemLibrary.get_engine_version(),'character_blueprint':path,'game_mode_blueprint':mode_path,'default_game_mode':mode_reference,'source_sha256':sources,'import_and_save':'completed','visual_rig_and_device_validation':'not_run'}
(receipt_dir/'avatar-import.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Imported, saved and activated male/female avatars in BP_AetheriaGameMode. Inspect pose, scale, axes, materials and each animation in UE5.6.')
print('Current animation adapter uses simple idle/walk/run loops. Production needs a BlendSpace/Animation Blueprint plus verified cast/dodge/hit clips and materials.')
