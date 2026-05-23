# Notes to Codex

Running notes from the human + Claude for Codex to read when it
picks the project back up. Add to the top; oldest entries fall off
the bottom. Keep it tight.

---

## 2026-05-23 — Fragment-57 seam fix bumped +1 → +2

### What Claude shipped this session

- Committed your existing Game Pak Check seam work
  (`f06debd diag: fragment 57 ...`).
- Added an `rdram_poke` companion to `rdram_peek` in
  `debug_server.cpp` so the next session can mutate Vtx coords on a
  live runner and see the result without rebuilding extras.c first.
  Used it to live-test overlap deltas.
- Bumped `FRAG57_SEAM_OVERLAP` in `extras.c` from your `+1` to
  `+2`. Verified visually: yellow row-seam dashes on every Game Pak
  Check card and the main-menu icon panels are gone. Your `+1` was
  the right shape; the magnitude was just under what RT64 needs at
  the user's render scale.
- Gated `[aspmain_hook] / [aspmain_chunk0]` per-audio-frame
  fprintfs behind `PSR_ASPMAIN_DEBUG=1`. Default silent.
- Gated `[pi] load_overlays(...)` per-DMA fprintf in
  `lib/N64ModernRuntime/librecomp/src/pi.cpp` behind
  `N64MR_PI_DEBUG=1`. **This is in the librecomp working copy, not
  in PokemonStadiumRecomp's tracked tree** — needs to be either
  upstreamed in that repo or replicated by the next setup. Worth
  doing because the prior unconditional spam drowned the runner's
  stderr.

### Key insight worth remembering

The main-menu icon panels (POKéMON STADIUM, Gallery, etc.) are not
in a different fragment — they reuse the same fragment-57 Vtx
grids (`section_addresses[239] + 0x8870` for the 2×8 panel) via
different MTX translations. So your existing `func_82D01758` hook
already fires for them; the same overlap constant applies. I
initially thought they were a separate fragment and burned time
hunting for it — don't repeat that.

### Still broken — leave for the next pass

- **POKéMON STADIUM panel bottom clip** on Main Menu. The central
  icon's bottom strip (the "POKéMON STADIUM" label band) is
  visibly truncated — descenders on the lowercase letters get cut
  off, and the gap between the label and the bottom corner-bracket
  art is shorter than it should be. This is **not** the same class
  as the seam bug. Quad #7 of the 2×8 grid at frag57+0x8870 is
  intentionally short (13 units tall vs 20 for the other rows in
  the column), so bumping seam overlap doesn't help. Likely causes
  to investigate, in priority:
  1. Y-axis scissor / projection clipping below some panel-local
     Y bound (RT64 viewport setup for the panel).
  2. The label texture is taller than quad #7 maps and the
     bottom rows of the texture are sampled outside the quad —
     check the V1/V2 t-coord in quad #7 (`0x8011D460 + 7*0x40`).
  3. RT64 handling of short-stride bottom-row quads at high
     render scales (similar in spirit to the +1 vs +2 issue, but
     in the Y dimension within a single tile).
- **Per-PC-runtime seam visibility**. The user's render scale
  needed +2; on a different display +2 might leave artifacts at a
  fractional sub-pixel position. If reports come in, the move is
  to make `FRAG57_SEAM_OVERLAP` settable via env (`PSR_SEAM_OVERLAP`)
  rather than chasing a perfect constant.
- **The original Player-3-specific yellow dashes** the user first
  reported were not a Player-3 thing — they tracked whichever card
  was currently selected (Player 1 with the Yellow cart, in their
  later capture). The +2 fix made all of them disappear. Don't
  spend cycles re-investigating per-port asymmetry.

### Useful artifacts in build/

- `state_back_01.png` — Main Menu post-fix (with the remaining
  STADIUM bottom-clip visible).
- `shot_v2_p1.png` — Game Pak Check P1 card post-fix, clean.
- `p1_zoom_before.png` / `p1_zoom_after.png` — before/after of
  the overlap bump, side-by-side proof the visual changed.
- `dl_*.bin` — captured DLs (still useful for offline GBI
  decoding). The decoder tools added this session live in
  `tools/_walk_dl_jumps.py` and `tools/_dl_color_audit.py`.

### Tool additions worth keeping

- `tools/_rdram_poke.py` — client for the new debug command.
  Includes a `--grid` mode that walks a Vtx-grid and applies the
  same row/col overlap math the in-game hook does, for live
  hypothesis testing.
- `tools/_screenshot.ps1` — captures the runner window by HWND.
  Codex should use this (or its own equivalent) on every render
  inquiry rather than reasoning from prior captures.
- `tools/_drive_to_mainmenu.py` — scripted navigation from boot
  through title + Game Pak Check + OK to land on Main Menu.
  Useful when you need a reproducible state without a human
  pressing buttons. Button names are case-sensitive
  (`Start`, `A`, `B`, `DUp`, etc. — see `button_bit` in
  `debug_server.cpp`).

---

## 2026-05-22 — Transfer Pak landing pass

You (Codex) added Transfer Pak support in
`src/main/transfer_pak.{cpp,h}` + the libultra-shim overrides for
`__osContRamRead` / `__osContRamWrite` / `__osPfsGetStatus`,
plus the `connected_device_info` change in `main.cpp` and the
config plumbing through `transfer_pak.cfg`. Nice work. A few things
the human and Claude touched up afterward — be aware of these:

### Docs are now consistent with the implementation

- `README.md` — the "Transfer Pak is not supported" callout is
  gone; there is now a Transfer Pak section documenting the cfg
  format, env-var overrides, and supported MBCs. The "Out of
  scope" section was reworded to clearly separate **Transfer Pak**
  (in scope, implemented) from **GB Tower** (still out of scope —
  the embedded GB emulator for free-play GB games).
- `CLAUDE.md` decision #4 was rewritten the same way. The old
  blanket "GB Tower out of scope; stub gb_tower/gb_mbc" framing
  used to also cover anything cart-bridge-related, which is no
  longer correct. **Do not re-stub Transfer Pak symbols.** Do still
  stub anything reachable from `gb_tower`, `gb_mbc`, or
  `gb_tower_roms`.

### Debug logging is now opt-in

The unconditional bus-trace `fprintf`s for block reads at
`0x600–0x60F` and control writes at `0x400 / 0x500 / 0x580` are
now gated behind `PSR_TRANSFER_PAK_DEBUG`. Default is silent (the
human dislikes log spam). The startup "loaded ROM …" and "port N
configured" one-shots, plus the error fprintfs (unsupported MBC,
save write failure, ROM too small), are left unconditional — they
are not spam and are useful at first launch.

**Do not** re-add unconditional per-block trace prints. If you
need more bus diag, add it under the same env gate.

### `Pak::RumblePak` is a deliberate stand-in

ultramodern's `connected_pak` enum is `{None, RumblePak}` — there
is no `Pak::TransferPak`. The TP code reports `RumblePak` only as a
"something is in the accessory slot" presence bit; Stadium does its
real pak-type discrimination over the bus through the
`__osContRam{Read,Write}` shims, so it should still see a Transfer
Pak signature there. The inline comment in `main.cpp` near
`get_connected_device_info` explains this — read it before changing
anything in that area.

If the SS Anne fork of N64ModernRuntime ever grows a real
`Pak::TransferPak`, switch the stand-in to it and the comment can go
away. That is the right upstream fix; don't paper over by adding
host-side TP-vs-Rumble discrimination here.

Note: real rumble output (`trigger_rumble` in `main.cpp`) is gated
on `controller_num == 0`. If a TP is configured on port 1, both
"TP present" and "rumble gets delivered" are true for that port. In
practice Stadium discovers the TP via bus reads and won't issue
motor commands on that channel, so it stays a theoretical concern,
but worth keeping in mind if rumble starts misbehaving while a TP
cart is loaded.

### Untracked, not committed

As of this note, `transfer_pak.cpp`, `transfer_pak.h`, the
`transfer_pak.cfg` sample, and the `main.cpp` / `extras.c` /
`CMakeLists.txt` / `.gitignore` integration edits all sit untracked
on `dev/sprite-corruption-menu-borders` alongside unrelated
sprite-corruption diag work. **Carve this out onto its own
branch** (suggested: `dev/transfer-pak`) before committing, so the
TP work has a clean history independent of sprite-corruption diag.

### What's tested / what isn't

The cfg-driven path is wired through `pkmnstadium::transfer_pak::initialize()`
in `main()` and the libultra shims will route bus traffic into it.
The end-to-end "Stadium reads my actual Yellow save and the cup
shows up with my real Pokémon" flow has not been verified yet —
needs a real-cart GB save dump to confirm. Multi-port (P2/P3/P4)
wiring exists but is also unverified. If you touch this, run Game
Pak Check against a known-good Yellow save and confirm the party
shows correctly before claiming anything works.

### Files involved (quick map)

| File | Role |
|------|------|
| `src/main/transfer_pak.h` | Public surface: init, has_transfer_pak, read_block, write_block |
| `src/main/transfer_pak.cpp` | Port FSM + cart emulator + cfg/env loader + libultra shims |
| `src/main/main.cpp` | `initialize()` call, `connected_device_info` gating |
| `extras.c` | Removed `UNIMPL_LIBULTRA` for the three shimmed entry points |
| `CMakeLists.txt` | Added `transfer_pak.cpp` to the target sources |
| `.gitignore` | Ignores `*.gb`, `*.gbc`, `transfer_pak.cfg` |
| `transfer_pak.cfg` | Local sample, gitignored |
