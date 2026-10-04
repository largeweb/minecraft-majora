# Shared memory ABI v3

The early sandbox/story sections document retained legacy modes. The packaged launcher uses native passthrough; its later revisions below override legacy control and block behavior.

2048 bytes, native little endian on this x86-64 machine. File permissions 0600. No credentials, ROM data, or game archives in shared memory. Header: u32 magic `0x434d4d31` at 0, u32 version `3` at 4.

All state values are 32-bit integers or IEEE floats. Every state section begins with a 32-bit sequence lock: odd during a write, even once published. The reader copies then validates the unchanged even sequence. All offsets below are absolute. C++ uses acquire/release atomic builtins; Java uses ByteBuffer VarHandles.

| Offset | Writer | Data |
| --- | --- | --- |
| 64 | 2ship | Port sequence |
| 68,72,76 | 2ship | Epoch, mode (0 off, 1 sandbox, 2 story), buttons |
| 80,84 | 2ship | Cumulative signed mouse X/Y |
| 88,92,96 | 2ship | Link position (live in story mode), port units |
| 100 | 2ship | Placement target valid |
| 104,108,112 | 2ship | Target cube's minimum corner, port units |
| 116,120 | 2ship | Scene ID, heartbeat frame |
| 192 | Minecraft | Player sequence |
| 196,200 | Minecraft | Epoch, heartbeat tick |
| 204,208,212 | Minecraft | Feet position (echo only in story mode), port units |
| 216,220 | Minecraft | Camera yaw/pitch in Minecraft degrees |
| 224,228,232 | Minecraft | Grounded, block present, block event (0 none, 1 place, 2 break) |
| 236,240,244 | Minecraft | Dirt minimum corner, port units |
| 320 | Minecraft | Collision request serial, release-published last |
| 324 | Minecraft | Request epoch |
| 328–348 | Minecraft | Feet X/Y/Z and desired delta X/Y/Z |
| 384 | 2ship | Matching reply serial, release-published last |
| 388–396 | 2ship | Allowed delta X/Y/Z |
| 448–460 | Test only | Magic `0x54455354`, buttons, cumulative mouse X/Y; ignored unless `COMPOSITE_TEST_INPUT` is set |

Buttons: W=1, S=2, A=4, D=8, Space=16, left mouse=32, E=64. Mouse counts use sensitivity 0.15 degrees/count. One block is 40 port units. +Y is up; Minecraft yaw 0 looks +Z. Minecraft's backing-world coordinates have a translation to keep its detached player over loaded chunks and above all backing terrain; those coordinates never cross the wire.

One port process holds an exclusive file lock. The collision request has one writer and at most one outstanding request. A response must match the request serial. Minecraft waits at most 200 ms; on timeout it stops the proposed movement and clears velocity. 2ship rejects the wrong epoch, nonfinite coordinates, and excessive displacement. Peers stop applying stale state after one second. A new world process changes the epoch; Minecraft rebuilds its detached player and clears the transient block.


## Story extension (mode 2)

Each extension begins with the same u32 seqlock. Payload offsets below are relative to the payload (sequence + 4).

- **512, port writer, 100-byte payload:** flags u32 at 0 (1 active, 2 dialogue, 4 native pause, 8 scripted control); initial yaw f32 at 4; 48 inventory item IDs as bytes at 8; 24 signed ammo counts at 56; native attack item u32 at 80; UI command sequence/kind/value u32 at 84/88/92; cumulative wheel i32 at 96.
- **768, Minecraft writer, 60-byte payload:** epoch, forwarded controls, selected hotbar index, inventory-open, camera mode (0 first, 1 rear, 2 front) u32 at 0/4/8/12/16; nine u32 bindings at 20; command acknowledgement at 56. Bindings 0–47 identify Zelda inventory slots; 253 is dirt, 254 native attack, 255 empty. Commands: kind 1 binds an owned inventory slot; kind 2 selects a hotbar index.
- Additional input bits: RMB 7; hotbar 1–9 at bits 8–16; Shift 17; Ctrl 18; Enter 19; Escape 20; F5 21; Tab 22; up/down/left/right arrows 23–26.

Story mode bypasses the sandbox collision RPC and detached LocalPlayer physics. Link's live position is authoritative in 2ship. Java publishes the camera, controls, inventory state and block layer; 2ship feeds native controller input before the game's normal cutscene/action gating and registers a DynaPoly collision mesh for the block. Epoch changes clear the transient block. Native save data stays in the story runtime.

Header diagnostic u32 values at 16/20/24/28 report raw keyboard bits, native game mode, native pressed buttons, and native held buttons. They are observational and not sequence-locked.


## Multi-block layer (ABI v3)

At 1024, Minecraft owns a seqlock plus a 208-byte payload: epoch u32 at payload 0, count at 4, remaining inventory count at 8, aimed index at 12 (0xFFFFFFFF for none), then 16 triples of float X/Y/Z at 16. Each triple is the minimum corner of a 40-unit dirt cube. 2ship validates count <=16 and finite coordinates, renders all active entries, and updates/removes matching native dynamic colliders. The old single-block fields mirror the first entry for sandbox compatibility. Both v3 peers must be restarted together.

Input diagnostics at header offsets 32/36 report mouse capture and native settings visibility. F8 now only recaptures in story mode. Focus gain, closing settings, and a gameplay key/click reacquire capture automatically. Scene changes clear the layer and refill its inventory.

## Native passthrough extension

The native profile uses the same ABI v3 and 2048-byte control file, with optional diagnostics: u32 flags at 40 (gliding=1, creative flight=2, GUI=4, grounded=8, swinging=16, elytra equipped=32), u32 Minecraft FPS at 48, and u32 last collision round-trip microseconds at 52, and source collision-poll count at 56. The `.rgba` sidecar is described in ARCHITECTURE.md. Only the native profile writes these optional diagnostic fields.

### Native block/combat revision

These extensions are used only with `COMPOSITE_FRAME` / `composite.passthrough=true`; earlier modes retain their behavior. Native mode uses a real LocalPlayer and a persistent empty dimension, not the old detached player. Scene origin in Minecraft is `(sceneId*1024, 1024, 0)`; one Minecraft block is 40 Zelda units.

- Header **44**, Minecraft writer: float current player collision height in port units, validated to 12–80 by the source.
- **1024** keeps its 208-byte block payload, but lists up to 16 nearby full-cube native colliders. Minecraft renders and saves the actual blocks; the source does not draw the old dirt textures. Count is not an inventory limit. Partial shapes are resolved by Minecraft for its player.
- **1280**, Minecraft writer, seqlock + 32-byte payload: epoch u32; camera x/y/z/yaw/pitch/FOV f32; valid u32. These are the actual Minecraft camera values, including third person.
- **1344**, Minecraft camera query serial; epoch at 1348; eye xyz at 1352–1360 and proposed camera xyz at 1364–1372. Source replies with matching serial at **1376** and resolved xyz at 1380–1388. One outstanding query; 50 ms timeout. Serviced with collision queries during frame pacing.
- **1408**, Minecraft writer, seqlock + 40-byte combat payload: epoch, sequence, kind u32; origin xyz and direction/velocity xyz f32; power f32. Kind 1 ordinary melee, 2 strong melee, 3 normal arrow. Arrow events originate from vanilla's server-side shot event, not from a guessed mouse duration.
- **1472**, source writer: processed combat sequence acknowledgement. **1476** melee events, **1480** confirmed native melee contacts, **1484** native arrows spawned. Counters alone do not prove enemy damage.
- **1504–1516**, Minecraft diagnostics: mining crack stage i32, hit-result enum u32, custom-dimension-active u32, held-stack count u32.
- **1536**, source writer, seqlock + 336-byte floor payload: epoch u32, minimum grid x/z i32, then 81 float floor heights in a 9×9 row-major grid. Invalid/below-world is -32000. Static scenery is sampled; mirrored user cubes are excluded. Minecraft places invisible floor supports for other entities, while its LocalPlayer ignores their collision and pick results.
- **1888**, source writer, seqlock + 148-byte observational enemy payload: epoch/count u32, then up to seven records `{actorId i32, xyz f32, health u32}` sorted by horizontal player distance. Used to verify native enemy damage in local playtests. Actor type IDs are not unique entity identifiers.

All extensions fit inside the existing 2048-byte file. The camera and combat payloads reject stale epochs and nonfinite/out-of-range values. Combat mutations run on the Zelda game thread. The Minecraft shot hook queues events from the integrated server; the client owns shared-memory publication.

### Instrument input

StoryPort flag **16** indicates an instrument/message screen. Existing control
bits remain unchanged: 23–26 are C-up/down/left/right, 19 is A, and Space (16)
also supplies A while native dialogue/instrument UI is active. The source
publishes an atomic timestamped pad snapshot for the audio controller poll;
it expires after 250 ms without a gameplay publication and clears on scene stop.
No audio-thread access to mutable PlayState is needed.

Only with `COMPOSITE_TEST_INPUT` and `COMPOSITE_TEST_SONG` enabled, the source
opens the real Song of Healing prompt for an isolated integration test.
Test command `0x534F4E47` at 448 reopens that prompt after a deliberately wrong
note; 2040 records the last audio pad and 2044 packs message mode/ocarina mode.
The packaged launcher clears these test flags. Historical local lesson fixtures are not included in this source package.

### Coherent native framebuffer camera

The `.rgba` sidecar has a 64-byte little-endian header: seqlock u32 at 0, magic `0x46524D31` at 4, width/height at 8/12, producer PID at 16. At 20 is camera epoch u32; x/y/z/yaw/pitch/FOV f32 at 24–44; valid/request sequence/scene u32 at 48/52/56; environment brightness f32 at 60. Bottom-up RGBA8 pixels follow at 64. The camera and pixels share one publication lock. Minecraft accepts a stable snapshot before camera culling and draws that same snapshot later in the frame. The source replaces view/projection matrices on every rendered interpolation frame.

Minecraft diagnostics at 1488/1492 are displayed image/request sequences, at 1496 requested-minus-displayed yaw f32 (transport latency, not layer misalignment), and at 1500 environment brightness f32. Control bit 20 is native B/cancel; bit 27 requests the owned ocarina during free movement.

### Native avatar and mask revision

Native mode adds a source-owned seqlock at **640** with a 28-byte payload: epoch/form u32 at payload 0/4, collision radius/height/eye height f32 at 8/12/16, native equipped mask and story flags u32 at 20/24. Form IDs are native: Fierce Deity 0, Goron 1, Zora 2, Deku 3, human 4. Minecraft applies dimensions to its client player and corresponding integrated-server player.

Minecraft writes a seqlock at **896** with 12-byte payload `{epoch, serial, inventorySlot}`. Source writes consumed serial at **924**. Consumption is not proof of successful equip: source checks ownership, gameplay state, and retains native restrictions. Valid mask slots are 24–47 from the existing StoryPort inventory. Equipping waits several native ticks before pulsing C-left so item-button availability can update. Form/status publication confirms actual transformation.

The mask item’s CustomData key is `composite_mask_slot`; its custom model is `composite:mask_<slot>`. Mask tokens are manually managed Minecraft items, supplied by the creative catalog; no inventory reconciliation or automatic refill runs. Tokens never write mask ownership back to Zelda.

Empty-hand native controls: with both hands empty and no Minecraft GUI, attack maps to bit 20 (B) and use maps to bit 19 (A). Held items retain Minecraft behavior. Short clicks are latched for publication; held buttons persist until release. GUI clicks are not forwarded.
