# Clock Town Composite

A local, experimental Minecraft Java + Majora’s Mask bridge. Both games run as native processes. Minecraft supplies controls, inventory, items and blocks; a patched 2ship2harkinian supplies Zelda scenery, story actors, collision and Link’s current form. Minecraft displays the combined frame.

**Tested only on x86-64 Arch Linux with Omarchy, using OpenGL and XWayland for 2ship.** This is a prototype, not a complete conversion or a multiplayer mod. No Steam, Proton or emulator is involved. See [platforms and limitations](docs/PLATFORMS.md) before trying another system.

This repository contains bridge source, patches and tooling. It does **not** contain Minecraft, ROMs, extracted game assets, textures, saves, account credentials, or prebuilt game binaries. Use your own licensed Minecraft account and supported, legally dumped Majora’s Mask ROM. Account login happens only in Prism Launcher.

## Versions

The compatibility pins in [versions.json](versions.json) match the working setup:

- Minecraft Java **26.3**, Fabric Loader **0.19.5**, JDK **25**.
- 2ship commit `e8757c14a0fc8701461b0458c9ed72c118bcfc67` and its pinned submodules.
- Version-specific Mixins: do not substitute the latest Minecraft/Fabric or upstream HEAD and assume compatibility.

## Start an already configured installation

From this folder:

```bash
./composite doctor       # report missing prerequisites
./composite start        # start the pair; does not duplicate a running pair
./composite status       # show only this configuration's game processes
./composite arrange      # repair the two-window layout on tested Omarchy
./composite restart     # gracefully stop and relaunch this pair
./composite stop         # stop this pair, leaving other applications open
```

Play in **Minecraft x Zelda — Native Passthrough**. The 2ship source window is kept on workspace `Zelda-renderer`. Its desktop image can look frozen: frames are captured directly and shown by Minecraft. Keep that process running. If Minecraft shows its experimental-world prompt, choose **Create Backup and Load**. `arrange` uses the tested Omarchy Lua dispatcher; it does not edit global desktop configuration.

**Save and Quit Minecraft before restarting when possible.** Shutdown waits up to 45 seconds and never force-kills. The prototype resumes Zelda from its Clock Town checkpoint, not arbitrary current position. See [saving](#saving-and-session-masks).

## First-time setup on Omarchy / Arch

Run the following from a terminal in the cloned repo. Generated files live under ignored `.local/`; private machine paths live in ignored `local.json`. Do not commit or distribute either.

### 1. Install tools

Install through Arch’s package manager (normal system upgrade included to avoid a partial upgrade):

```bash
sudo pacman -Syu --needed base-devel git cmake ninja python lsb-release \
  sdl2 sdl2_net libpng libzip nlohmann-json tinyxml2 spdlog \
  libogg libvorbis opus opusfile libglvnd glew \
  jdk25-openjdk prismlauncher xorg-xwayland
```

Package availability changes on rolling Arch. If the pinned JDK is unavailable, obtain JDK 25 from a trusted official JDK distribution and set `java_home` in `local.json`. The build and runtime require Java 25; changing Minecraft to fit a different JDK is not a supported workaround. Upstream references: [2ship building instructions](https://github.com/HarbourMasters/2ship2harkinian/blob/e8757c14a0fc8701461b0458c9ed72c118bcfc67/docs/BUILDING.md), [Prism Linux installation](https://prismlauncher.org/download/linux/).

### 2. Set up your Minecraft account and pinned instance

```bash
./composite init
./composite prism
```

`init` creates a separate Prism data directory and an instance named **Clock Town Composite**. In Prism:

1. Add your own Microsoft account and complete Microsoft’s sign-in flow. No passwords or tokens go in these scripts or the repo.
2. Launch **Clock Town Composite** once. Prism obtains the pinned Minecraft/Fabric libraries using your account. Reach the Minecraft title screen, then quit the client. Keep Prism open or close it.
3. If Java detection fails, select your JDK 25 in Prism and update `java_home` in `local.json` for the build tools.

This project does not implement authentication, bypass ownership, or supply an offline account. Do not copy someone else’s Prism directory.

### 3. Fetch and build the patched port and Fabric mod

```bash
./composite fetch-source
./composite build --jobs 8
```

`fetch-source` clones the official 2ship repository and pinned submodules. `build` applies the two patches, installs the bridge hooks into that checkout, builds 2ship and its port archive, and compiles the Fabric mod against your local Minecraft libraries. CMake may fetch open-source build dependencies. **No script downloads a ROM or game assets.** The dirt texture header is generated privately from your installed Minecraft client, never shipped in Git.

The first build can take several minutes and substantial disk space. Use fewer jobs if memory is constrained. `build --offline` is for a previously populated dependency cache, not a fresh clone.

### 4. Supply your own ROM and extract locally

Put your supported, big-endian dump at exactly:

```text
.local/runtime/mm.z64
```

That is next to `.local/runtime/2ship-passthrough.elf`. This is our documented filename; upstream scans supported ROM extensions rather than requiring this basename. `.m64` is **not** one of the ROM extensions accepted by the pinned extractor. Supported `.n64` / `.v64` dumps are also recognized by upstream; do not merely rename a different file format to `.z64`.

The pinned source’s `docs/supportedHashes.json` lists supported dumps. Compare your local dump using `sha1sum`; the extractor also validates it. See the [official compatibility guidance](https://github.com/HarbourMasters/2ship2harkinian/blob/e8757c14a0fc8701461b0458c9ed72c118bcfc67/README.md). This repo provides no ROM source or download instructions.

```bash
./composite extract
```

Accept 2ship’s **Generate** prompt and select your dump if asked. It creates `mm.o2r` locally. Close the native game once extraction finishes. Then:

```bash
./composite assets
./composite install
```

This generates the sword/mask resource pack from your own `mm.o2r` and a dimension datapack using your installed Minecraft. Generated assets stay private. Stone sword uses the **Kokiri Sword** icon (not the Master Sword); diamond sword uses the **Great Fairy’s Sword** icon.

### 5. Create the private Minecraft world

Open `./composite prism`, launch the instance, and create a **single-player** world:

1. Name it exactly **Composite_World**. Use Creative and allow commands for initial testing; Survival is also supported by the bridge.
2. During world creation, open **Data Packs → Open Pack Folder**. Copy the whole generated folder `.local/generated/clocktown-native` into that folder. Return and enable the pack before creating the world. Confirm the experimental settings warning.
3. In Minecraft resource packs, enable **Local Zelda Swords**.
4. Save and Quit, then close Minecraft. Confirm the world folder is `.local/prism/instances/clocktown-composite/minecraft/saves/Composite_World/`.
5. Run `./composite install` again while Minecraft is closed. This ensures the world’s datapack and mod are installed at the expected paths.

Do not use an existing campaign world: this mod changes movement and dimensions and is intended for its dedicated local world. No world or seed-dependent assets are supplied.

```bash
./composite doctor
./composite start
```

The mod opens `Composite_World` automatically. With no native checkpoint, `start=auto` begins the prototype’s Song of Healing setup in the Clock Tower (including the required ocarina). Finish the lesson and exit into Clock Town. Once a safe post-lesson checkpoint exists, subsequent launches resume outside the tower. This bootstrap is not the original intro or a promise of full campaign compatibility.

## Controls

| Key | Function |
| --- | --- |
| WASD / mouse | Move / look |
| Space / Ctrl / Shift | Minecraft jump / sprint / sneak |
| E | Minecraft inventory |
| F5 | First person / third-person views with Zelda’s avatar |
| Left / right click, **both hands empty** | Zelda B / A, using the current form/context |
| Left / right click, holding an item | Minecraft attack/mining / item use or placement |
| Right-click a held mask | Request native equip/transformation; same transformation mask returns to human |
| Enter / Backspace | Explicit Zelda A / B even when holding an item |
| Tab | Zelda pause menu |
| O | Ocarina if owned in Zelda |
| Arrow keys / Space | C-button notes / A during native instrument screens |
| Escape | Minecraft menu |
| T or `/` | Minecraft chat / commands |

Masks are optional inventory items. Find all 24 in Creative → Search → `mask` (also Tools & Utilities). Drop, store or delete them freely: nothing refills or forces inventory slots. Zelda ownership still gates equip. Giant’s Mask and Fierce Deity retain native location restrictions. Empty-hand mining is replaced by B; select a tool to mine blocks.

## Saving and session masks

Minecraft blocks and inventory save in its dedicated world. Zelda and Minecraft **do not have a merged campaign save**. The bridge writes a native `composite-checkpoint.json` on reaching Clock Town after Song of Healing, once per process. Launch resumes that checkpoint at the tower entrance and as human Link. Do not expect later quest progress, position or time-cycle state to be restored by this launcher.

For an optional all-masks sandbox after you have your own Clock Town checkpoint:

```bash
./composite stop
./composite session-masks
./composite start
```

This creates a separate private session copy and unlocks ownership of all 24 masks there. Get the actual items from Creative → Search → `mask`; they are never forced into your inventory. The original checkpoint is not edited. To return, stop, then set `runtime` back to the recorded `base_runtime` in `local.json`. Native location restrictions still apply.

There are no saves or unlocks in this repository. The original working machine has a separate all-masks session copy; its private `local.json` can point at that runtime. A normal clone starts without that session. An inventory item alone does not grant Zelda ownership. Preserve your own checkpoint outside the repo before experimenting. Runtime files belong under `.local/` and must never be pushed.

## Troubleshooting and updating

- Missing masks/textures: run `assets`, install while Minecraft is closed, and enable the resource pack. It must come from your own local assets.
- “Composite dimension missing”: close the world, install the datapack into `Composite_World/datapacks/clocktown-native`, then reopen the world.
- A/B do not fire from mouse clicks: empty **both** hands, or use Enter/Backspace.
- Windows misplaced: `./composite arrange`; if the desktop is locked, unlock and retry. It refuses to move a window unless it can confirm focus.
- Poor frame rate: keep the source window at 1280×720 and OpenGL. Do not fullscreen the hidden renderer. Logs are private under `.local/logs/`.
- After source edits: `./composite build-fabric`, then close Minecraft and `./composite install`. Native changes require `./composite build` and restarting both. Do not upgrade pins casually.
- `start` does not replace a partly running session. Use `restart` to obtain a clean pair.

## Development and sharing

```bash
./composite build-fabric
./composite test
./composite audit
```

Tests cover shared-memory ABI, legacy block collision helpers and launcher isolation. They are not a full story, all-form ability or performance certification. [Validation notes](docs/VALIDATION.md) distinguish existing gameplay checks from packaging checks.

Push only this source repository:

```bash
git remote add origin YOUR_REPOSITORY_URL
git push -u origin main
```

On a fresh clone, run setup above. Keep `local.json`, `.local/`, generated jars/textures, ROMs, extracted archives, account files and worlds out of Git. `audit` checks the Git index before sharing. To create a clean source archive after committing:

```bash
git archive --format=tar.gz --output=../clocktown-composite-source.tar.gz HEAD
```

See [architecture](docs/ARCHITECTURE.md), [ABI](docs/PROTOCOL.md), and [third-party notices](THIRD_PARTY.md). This is an unofficial experimental project, not endorsed by Nintendo, Mojang/Microsoft, Prism Launcher or HarbourMasters.
