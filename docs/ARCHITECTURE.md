# Architecture

Minecraft is the interactive process. Fabric Mixins retain the local player, hands, hotbar, creative inventory, commands and block renderer. Zelda provides terrain collision, story actors, Link’s current form and cutscene camera. Both must remain running.

The patched port captures an OpenGL backbuffer on its render thread. A 2048-byte control file and an RGBA sidecar under `/dev/shm` exchange movement, camera, events and frames. Sequence locks prevent torn snapshots; epochs reject stale sessions. Native collision queries are serviced during frame pacing to reduce latency. A late-camera hook reduces rotation lag. Minecraft composites the captured background and its own block/item layers; this is not a shared-depth renderer.

- `fabric/src/`: Java mod and Mixin descriptors. Compiled only against the user’s locally installed pinned libraries.
- `ship/`: source-side bridge headers and hook implementation. The generated `DirtTexture.h` is deliberately absent from Git.
- `patches/2ship.patch`: native HUD suppression, late-camera update, instrument input hook.
- `patches/libultraship.patch`: render capture, collision servicing while pacing, and offline cached-header support.
- `tools/`: pinned source/build steps, local extraction, private config, scoped launcher and optional Omarchy layout helper.
- `versions.json`: exact tested source/toolchain integration pins.

Native dimensions apply to both the Minecraft client and its integrated-server player. Invisible floor supports serve entities/items but are excluded from the player’s collision to avoid client/server ground jitter. Actual Minecraft blocks remain additional colliders.

Mask items contain a `composite_mask_slot` tag and a custom model identifier. The creative catalog lists 24. Inventory contents are never reconciled/refilled automatically. On use the bridge requests an owned native mask; Zelda handles equip/transform restrictions. Dropping an item does not discard native ownership.

The port exports only the displayed frame and gameplay state, not credentials. Prism owns account authentication. Run only one pair per configured shared-memory path. `start` is non-destructive when a session already exists; `restart` uses scoped graceful termination. Neither reads nor prints authentication arguments.

The launcher’s PID discovery checks the native binary plus runtime working directory, and Java’s bridge argument plus exact Minecraft working directory. Prism itself and unrelated games are not stopped. A per-repo launcher lock prevents simultaneous start/stop operations. Native file locking prevents duplicate producers.

For shutdown, use Minecraft Save and Quit first, then `./composite stop`. A process that does not exit after SIGTERM is reported instead of forcibly killed. Checkpoint limitations are documented in the main README.
