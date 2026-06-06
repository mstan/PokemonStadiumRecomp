# Graphics options

The game ships looking good with **no configuration** — 4× MSAA
anti-aliasing is on by default so the N64 models don't have jagged
edges, and the 2D menus and HUD stay sharp.

Everything below is **optional** and aimed at people who want to push
image quality further or tune for a specific GPU. All of it is read at
startup from environment variables in
[`src/main/rt64_render_context.cpp`](../src/main/rt64_render_context.cpp);
all of it is off / default unless you set it.

## Anti-aliasing

`PSR_RT64_MSAA` — sets the multisample anti-aliasing level. Accepts
`None`, `2X`, `4X`, `8X` (or `0`, `2`, `4`, `8`).

- Default: **4×**. This smooths the models' polygon silhouettes (the
  hard, jagged edges). It's the right default for most GPUs.
- Lower it (`2X`/`None`) on a weak GPU; raise it (`8X`) if you have
  headroom.

MSAA alone does **not** fix the rainbow speckle you can see on very thin,
far-away geometry (e.g. a Pokémon's antennae at distance) — those
triangles are narrower than a pixel, so they need supersampling below.

## Supersampling (higher internal resolution)

Supersampling renders the 3D at a **higher** resolution than the window,
then shrinks it back down. The shrink averages several rendered pixels
into each output pixel, which cleans up the thin geometry MSAA can't.

Two knobs work together:

- `PSR_RT64_RES_MULT=<1.0–16.0>` — how much higher than the window to
  render. `2` means render at double the window's resolution.
- `PSR_RT64_DOWNSAMPLE=<1–8>` — how much to shrink the rendered image
  back down before presenting it.

The image you actually see is sized `RES_MULT / DOWNSAMPLE` relative to
the window. The cleanest results come from pairing them so the output
lands exactly on the window's resolution — that way the 3D is
supersampled but the final image isn't stretched or squashed when it's
shown.

**Example.** At a 720p window:

```
PSR_RT64_RES_MULT=6
PSR_RT64_DOWNSAMPLE=2
PSR_RT64_MSAA=4X
```

renders the scene at 1440p, outputs at 720p (6 ÷ 2 = 3× the window, then
presented to a 3×-native window with no extra rescale), and 2×2-
supersamples the 3D. The menus are untouched.

> **Why the menus survive this.** A single screen is built from several
> render layers of different sizes. RT64 used to decide the
> high-resolution-scale reduction separately for each layer, so a tall 2D
> menu layer ended up at a different scale than the layers it composites
> with, and the menus drew over-scaled and clipped. That's fixed: the
> scale is now decided once per frame from the tallest layer and applied
> to all of them, so every layer matches and the menus stay correct at any
> internal resolution. (PSR issue #12.)

## Other knobs

- `PSR_RT64_UPSCALE2D=All` — also upscale 2D elements (default leaves 2D
  at native resolution).
- `PSR_RT64_FILTERING=Nearest|Linear` — texture filtering mode.
- `PSR_RT64_THREEPOINT=0|1` — N64-style three-point texture filtering.

## Quick recommendations

- **Just want it to look good?** Do nothing. The defaults are tuned.
- **Strong GPU, want it crisper?** Try `PSR_RT64_RES_MULT=2` with
  `PSR_RT64_DOWNSAMPLE` left at default, then experiment.
- **Weak GPU / low frame rate?** Set `PSR_RT64_MSAA=2X` or `None`.
