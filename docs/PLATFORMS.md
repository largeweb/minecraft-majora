# Platform status

| Platform | Status for this composite bridge |
| --- | --- |
| x86-64 Arch Linux / Omarchy | Only environment with observed gameplay. Two native processes, 2ship OpenGL through XWayland, Minecraft client renders the combined view. |
| Other Linux distributions/desktops | Plausible, **untested**. Replace package commands and window arrangement; verify JDK 25, graphics drivers, shared-memory permissions, and equivalent native libraries. The optional Lua Hyprland dispatcher is specific to the tested Omarchy installation. |
| Windows | **Not supported by these scripts/source as packaged.** Requires replacing POSIX mmap/flock/getuid transport and `/proc` process discovery, GNU weak-symbol integration, launcher paths, and window handling; validate OpenGL frame capture. No Proton testing. |
| macOS | **Untested and requires porting.** POSIX APIs help, but `/dev/shm`, `/proc`, XWayland launch assumptions, source packaging and default Metal renderer differ. The frame bridge explicitly reads an OpenGL backbuffer; native Metal support does not make this bridge work. |
| ARM Linux / Apple Silicon | **Untested.** Byte-order/atomic ABI assumptions, native dependencies, and driver behavior must be verified. |

Upstream Minecraft and 2ship supporting other operating systems does not establish compatibility of this bridge. Upstream 2ship lists its own render backends in its [README](https://github.com/HarbourMasters/2ship2harkinian/blob/e8757c14a0fc8701461b0458c9ed72c118bcfc67/README.md). The table above describes this integration, not upstream support.

“Native” here means Minecraft runs on the JVM and 2ship is compiled machine code on Linux. It does not mean the mod is cross-platform or that both games share one renderer. Frames cross CPU memory, and there is no shared depth buffer. Performance on the tested machine is not a guarantee for other hardware.

Known prototype limits: incomplete campaign/time-cycle save integration, incomplete form-specific abilities and merged health/combat, separate Minecraft and Zelda state, no multiplayer, and no guarantee that every quest/area or block/entity interaction works. Native mask restrictions remain. Existing old sandbox code is retained in source but not the packaged launch mode.
