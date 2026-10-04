# Third-party notices and asset boundaries

- 2ship2harkinian: official source fetched at the revision in `versions.json`. Upstream license is CC0-1.0; the license text is retained in `LICENSES/2ship-CC0.txt`. The 2ship patch includes minimal upstream context.
- libultraship: upstream submodule is MIT-licensed; see `LICENSES/libultraship-MIT.txt`. The renderer patch includes upstream context. Its other dependencies retain their own licenses in the fetched source.
- Minecraft Java, its libraries/assets, and Fabric are obtained through the user’s own Prism instance. They are not bundled. Fabric and Mixin dependencies retain their own upstream licenses.
- Majora’s Mask ROM data, extracted archives and mask/sword images are not bundled. Tools read the user’s local dump-derived archive. Minecraft UI/dirt/dimension data is likewise generated from the user’s installed client where needed. Do not distribute generated assets as part of this project.

The root MIT license applies to the original bridge/tooling code. It does not license any third-party game, trademark, account, asset or dependency, and does not replace the upstream notices above. No affiliation or endorsement is claimed.
