# Pokémon Stadium Recompiled v0.4.6-beta — shared ImGui launcher

This release replaces the old RmlUi SS Anne launcher with the shared Dear ImGui
`recomp-ui` N64 launcher. The launcher no longer keeps the old RmlUi overlay
stack active while waiting at the pre-boot screen, addressing the idle launcher
CPU behavior reported in issue #19.

## Launcher

- New DPI-independent N64 layout with ROM selection, controller configuration,
  Transfer Pak settings, and the Play action kept accessible.
- Uses RT64's existing Dear ImGui copy instead of linking a second copy.
- Includes a corrected host-ImGui CMake integration in the pinned `recomp-ui`
  revision.

Game compatibility and the remaining announcer-audio caveat are unchanged from
v0.4.5-beta.
