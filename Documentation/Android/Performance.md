# Mobile performance verification

There is no native Android measurement or APK in this milestone. A Samsung Galaxy A34 (SM-A346B), 6GB RAM, at30FPS is a proposed baseline device, not a tested device. Start with mobile forward rendering, Lumen/Nanite off, one shadow cascade, a256MB streaming pool and constrained content resolution.

Runtime engineering geometry is split into HISM batches per building. This narrows visibility bounds compared with one whole-city instance batch. The actual benefit requires UE profiling; source inspection does not establish a frame-rate gain. Licensed FBX assets are unimported and have no verified mobile LOD/material budget yet.

The browser prototype's headless software-renderer samples were approximately1FPS near the guide (206draw calls,1,024,950submitted triangles) and3FPS at the bank (166calls,1,012,230triangles). Those samples cannot establish smooth native or physical-device play. Whole-district instance bounds are one plausible browser cost, not a proven GPU diagnosis.

After a real UE Android package and installation, capture the hospital, awakening crystal, trial spell spam, Guardian phase2 and crowded city route with `stat unit`, `stat rhi` and CSV profiling. Record device/model/OS, resolution/quality tier, build commit, median/p95/p99 frame time, draw calls, triangles, memory, thermal state and time since launch. Check touch input, keyboard text entry and return from background on the same device.

Proposed initial gates: stable30FPS over a10minute route, p95 frame time under40ms, bounded memory and no crash/texture fallback. These are targets. The CI receipt must never label skipped device captures as passes. Finish skeletal LODs, texture compression, materials and authored lighting before reassessing the budget.
