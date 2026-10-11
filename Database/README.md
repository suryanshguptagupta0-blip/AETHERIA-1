# Persistent storage acceptance criteria

The current native implementation serializes validated, versioned Rules state and bounded NPC conversations into server-owned Unreal SaveGame slots. No SQL schema has been deployed.

A production replacement must store account ownership, versioned character snapshots, registered spell definitions, chapter choice, NPC memory and immutable money transfers. Deposit/withdraw and merchant purchases must commit atomically with an idempotency key, reject integer overflow and preserve conservation. Concurrent sessions require a unique active lease per character. A migration must preserve the versioned rules decoder and be exercised against truncated/corrupt and old save fixtures before deployment.
