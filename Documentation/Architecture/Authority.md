# Authority and persistence boundary

This source milestone targets Unreal Engine 5.6. It has not been compiled with UnrealHeaderTool or run as a dedicated server in this workspace.

The native Rules module is ordinary C++ and can be compiled separately. Unreal player state owns that rules state on the authority. The owning client receives a small replicated public profile; client UI cannot write mastery, rewards, balances, awakening, chapter or registered spells. Cast input submits intent; the server resolves approved spell parameters and emits a projectile whose collision consumes a single-use cast proof. Guardian damage and trial progress derive from those proofs.

NPC messages are bounded authored conversations. No external AI API or billing is used. Chat history and familiarity are persisted independently of paid services. A service is a separate, explicit RPC, validated against the nearby NPC role and rules resources. Text claiming to deposit, heal, buy, train or defeat a boss cannot execute that action.

A local single-player identity is a generated GUID saved on the server. Multiplayer sessions receive fresh server GUIDs. Development profile IDs are disabled by default, permit controlled LAN reconnect checks when explicitly enabled, and must never be treated as authentication. Duplicate active development IDs are rejected. Production accounts, verified session tokens, cross-server persistence and account recovery are unfinished and must be implemented before public multiplayer release.

Versioned rules serialization validates bounds and integrity before replacing active state. Saves live under Unreal's Saved/SaveGames directory. This is suitable for a standalone or one server development run; it is not a transactional hosted database, distributed lock or production backup system. The separate Database and Backend notes describe required replacement work. Do not accept a save file uploaded by an untrusted client.

Replication and C++ intent guards are source evidence only. The acceptance gate remains an actual UE dedicated server with 4–8 clients, packet loss/latency, reconnect, simultaneous services and hostile RPC tests.
