# Validation scope

This package was assembled from a working local composite session on **x86-64 Arch Linux / Omarchy**. No other OS has been tested. The running installation was not replaced during packaging.

Observed gameplay in the existing installation includes native scene rendering, Minecraft block placement/breaking, native terrain collision, F5 Link/form views, creative mask icons, native mask transformations, and empty-hand A/B forwarding. Dropping a mask was checked after removing automatic refill. These observations do not establish that every Minecraft feature, Zelda quest, form ability, or save cycle works.

Packaging checks:

- Fabric source rebuilt against the installed pinned Minecraft 26.3/Fabric 0.19.5 libraries with JDK 25.
- C++/Java shared-memory layout and collision request/reply test passed.
- Legacy block VoxelShape side/top/underside/slide/break regression test passed; this is not an end-to-end native movement test.
- Launcher unit tests cover duplicate launch refusal, process-identity revalidation and scoped SIGTERM, isolated defaults, non-overwriting instance creation, and creating an all-masks copy without changing the original checkpoint.
- `doctor` and read-only process discovery checked against the existing running installation.
- Local asset tooling regenerated 24 mask icons, 2 sword icons and the 2032-block-high dimension datapack. Generated files are excluded from the source package.
- Packaged Java and native gameplay source matched the current working project byte-for-byte.
- Git-index audit passed for all 64 source/documentation files: no known game assets, credentials, saves, private home paths, symlinks or generated binaries.
- Clean native build passed: 1,872 build steps, including the packaged 2ship/libultraship patches, generated local dirt header, `2ship` and `Generate2ShipOtr` targets. Validation used clean local Git clones of pinned source plus an existing dependency cache with network fetching disabled. It did not install over the running native binary.

A fresh Microsoft sign-in, new-ROM extraction UI, and first-time world creation were not repeated with a second account. They use the pinned upstream launcher/extractor workflows and are documented as setup steps. No ROM, game files or credentials were downloaded or included during packaging.
