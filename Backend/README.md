# Backend status

There is no deployed backend, paid service or production account provider in this milestone. The Unreal authority handles gameplay and server-local SaveGame persistence. Browser prototype saves remain browser-local and are not native accounts.

Before public multiplayer: implement verified login tokens, map account identity to server-only profile IDs, load/save through transactional storage, enforce per-profile session leases, retain an economy ledger, provide backups and recovery, and test reconnect across different server processes. These are explicit remaining tasks, not running services.
