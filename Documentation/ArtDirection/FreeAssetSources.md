# Free character and animation source audit

Audit date: 2026-10-11 UTC. Status: **source assets downloaded, not imported or integrated**. Two MIT-licensed base character FBX files, six idle/walk/run FBX files, their license notices, and a free Noto Sans Devanagari TTF with its license were downloaded. No textures, `.uasset` files, or paid assets were acquired. These raw sources are not Unreal runtime assets. The current runtime geometry remains an engineering placeholder; this document does not establish a realistic playable build.

## Verified sources

| Source | License / price evidence | Available content | Current status |
| --- | --- | --- | --- |
| [Microsoft Rocketbox](https://github.com/microsoft/Microsoft-Rocketbox/tree/0943055db6ec570bcef9f2c8b41c9e5467c808f9) | Repository `LICENSE.md` is MIT, copyright Microsoft 2020. README explicitly says the avatar library changed to MIT in December 2020. Preserve the copyright and permission notice with redistributed assets. | README describes 115 fully rigged characters and compatible animations. The pinned tree contains male/female adult FBX meshes, texture files, facial variants and 471 FBX files under `Assets/Animations`. README's historical release announcement says 417 animations; the tree count is a file count, not a deduplicated clip count. | Public files and metadata verified. Two base mesh and six animation FBX sources are now downloaded under `Content/Characters/SourceArt` and `Content/Animations/SourceArt`; all eight match upstream Git blob SHA and exact byte counts. They are not imported, retargeted or integrated. |
| [Epic MetaHuman](https://www.metahuman.com/en-US/license) | Current official license page says MetaHuman is included under the standard Unreal Engine license and is free for users making under USD 1 million in revenue; standard UE terms apply above that threshold. This is an Epic license, **not MIT**. [UE license](https://www.unrealengine.com/en-US/license), [UE EULA](https://www.unrealengine.com/en-US/eula/unreal). | Official site describes fully rigged photorealistic digital humans. Individual character download sizes, rigs and animation assets were not acquired or inspected. | License page retrieved. No Epic account session, engine installation, MetaHuman character or content download. Treat as a later art candidate, not an integrated asset or a perpetual zero-cost license. |

The audited Rocketbox repository is archived and its pinned commit is `0943055db6ec570bcef9f2c8b41c9e5467c808f9`. Its full Git tree is about **24.23 GB** of listed blob data. A whole-repository asset download is unnecessary for the first male/female milestone.

## Pinned Rocketbox candidates

URLs below use the exact audited commit. All eight listed files are downloaded as source art. No textures or facial variants were downloaded. Total FBX source bytes: 7,642,016. Import and runtime integration still require Unreal Editor.

| File, relative to repository root | Bytes | Purpose |
| --- | ---: | --- |
| `Assets/Avatars/Adults/Female_Adult_01/Export/Female_Adult_01.fbx` | 609,152 | Female skeletal mesh candidate |
| `Assets/Avatars/Adults/Male_Adult_01/Export/Male_Adult_01.fbx` | 521,472 | Male skeletal mesh candidate |
| `Assets/Animations/all_animations_max_motextr_static/f_idle_breathe_01.max.fbx` | 1,379,504 | Female idle candidate |
| `Assets/Animations/all_animations_max_motextr_static/m_idle_breathe_01.max.fbx` | 1,397,280 | Male idle candidate |
| `Assets/Animations/all_animations_max_motextr_xy/f_walk_neutral.max.fbx` | 1,002,832 | Female walking candidate |
| `Assets/Animations/all_animations_max_motextr_xy/m_walk_neutral.max.fbx` | 958,512 | Male walking candidate |
| `Assets/Animations/all_animations_max_motextr_xy/f_run_neutral.max.fbx` | 895,504 | Female running candidate |
| `Assets/Animations/all_animations_max_motextr_xy/m_run_neutral.max.fbx` | 877,760 | Male running candidate |

Raw file URL prefix: `https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/0943055db6ec570bcef9f2c8b41c9e5467c808f9/`.

`Female_Adult_01`'s complete Export + Textures set totals 107,697,491 bytes; the equivalent male set totals 94,758,724 bytes. These totals include optional facial FBX, wrinkle textures and legacy material files. Base mesh-only downloads are approximately 1.13 MB combined; textured, animated characters need additional content. Texture entries include large uncompressed TGA files; these source sizes do not predict Android cooked size.

Binary inspection confirmed a standard FBX header in both meshes, FBX version 7400, `Bip01` limb names, skin data and 80 `Cluster` markers per file. The sampled idle FBX is version 7700 with `Bip01` limb names and animation-stack markers. Counts of text markers are not validated skeleton bone counts. Matching limb names support a compatibility candidate; they do not establish UE5 import or retarget correctness.

## Import and integration milestone

1. Preserve `References/Rocketbox-LICENSE.md` alongside imported/redistributed assets. Fetch the chosen files by pinned URL and compare Git blob SHA entries in `References/Rocketbox-selected-manifest.json`; record any edited source asset separately.
2. In UE 5.6, import each selected base FBX as a Skeletal Mesh with animation import disabled. Inspect reference pose, bone hierarchy, mesh LOD groups, material slots, centimeters/scale and forward axis. Keep a separate skeleton if UE rejects sharing; identical bone names alone are insufficient evidence.
3. Build mobile materials from color and normal maps. Author roughness/opacity explicitly; legacy specular maps and Unity `.mat`/`.shader` files are not native UE materials. Compress and limit texture resolution using the actual target-device budget.
4. Import selected animation FBX against the validated skeleton, inspect scale and reference-pose warnings, and play each sequence. Set up IK Rig/IK Retargeter if adopting Manny/Quinn animations. Rocketbox uses `Bip01` naming, not the UE mannequin skeleton. No direct mannequin compatibility is claimed.
5. Validate the XY locomotion files for translation/root motion. Convert to in-place sequences or configure authoritative root motion deliberately so animation does not fight CharacterMovement prediction. Build idle/walk/run BlendSpace and Animation Blueprint, then add verified aim/cast/dodge/hit animations; the current selection does not satisfy the full combat animation list.
6. Bind the male/female meshes to the character-selection state without changing server-owned affinity or progression. Test socket placement, capsule fit, foot placement, third-person camera, cloth/hair cost and replicated locomotion before replacing the placeholder character.
7. Cook Android ARM64 and measure on the chosen physical device. Photoreal appearance, material quality, skeletal animation performance and native touch controls require actual UE and device execution.

The upstream [Unreal importer README](https://github.com/microsoft/Microsoft-Rocketbox/blob/0943055db6ec570bcef9f2c8b41c9e5467c808f9/Tools/Unreal/Import/README.md) targets UE 4.24/4.25 and was tested on Windows 10. It explicitly lists an unfinished opacity-material issue. Its Python/material files must not be treated as a verified UE5.6 importer. Manual import above is a proposed integration procedure, not a completed test.

## Evidence saved locally

- `References/Rocketbox-LICENSE.md`: exact upstream MIT notice.
- `References/Rocketbox-README.md`: exact upstream library description and historical updates.
- `References/Rocketbox-Unreal-Importer-README.md`: exact upstream importer requirements and limitations.
- `References/Rocketbox-tree.json`: full, non-truncated Git tree and pinned commit.
- `References/Rocketbox-selected-manifest.json`: candidate files, Git blob identifiers and exact source sizes.
- `References/Rocketbox-probe.json`: bounded in-memory binary inspection results and SHA256 hashes; FBX binaries were discarded.
- `References/MetaHuman-License-Page.txt`: brief visible license-page excerpt, retrieval date, source URL and original HTML SHA256.

Unreal Editor, UHT, cooking, retargeting, character animation playback and physical Android performance were unavailable in this environment. No asset import, rig integration, visual quality, APK or runtime animation pass is claimed. See `References/Rocketbox-downloaded-source-manifest.json` for the eight retained FBX files, source URLs, Git blob SHA, SHA256 and exact sizes; both source folders retain the MIT license.

## Hindi font source

[Google Fonts Noto Sans Devanagari](https://github.com/google/fonts/tree/bd8f81ddb5c74d5c8897b36ad88b440266245103/ofl/notosansdevanagari) was verified at commit `bd8f81ddb5c74d5c8897b36ad88b440266245103`. `OFL.txt` names Copyright 2022 The Noto Project Authors and SIL Open Font License 1.1. The OFL permits bundling and embedding with software while preserving the license; fonts cannot be sold by themselves.

Downloaded files are `Content/UI/Fonts/NotoSansDevanagari-Variable.ttf` (641,944 bytes) and `Content/UI/Fonts/OFL.txt` (4,386 bytes). The exact URL, Git blob SHA and SHA256 are recorded in `References/NotoSansDevanagari-manifest.json`. They are free source files, **not imported, integrated or cooked**. fontTools successfully parsed the TTF and confirmed all non-space codepoints in `मैं आपकी सहायता कैसे कर सकता हूँ?` are present; this validates glyph coverage only, not shaping or UMG rendering.

Create a UE Font/Composite Font asset, establish a supported face (generate a static weight instance from the variable font if the target UE import path requires one), and configure Devanagari fallback for U+0900–U+097F plus punctuation/Latin fallback. Assign the composite asset to every UMG Hindi text and input widget. Verify full text shaping and combining marks with actual Slate/UMG rendering, for example `मैं आपकी सहायता कैसे कर सकता हूँ?`, at mobile sizes. Glyph availability alone does not prove shaping or rendering. Package the font asset and retained OFL notice in the Android cook, then inspect it on device. The existing Hindi translation table and downloaded TTF do not establish working Hindi typography.
