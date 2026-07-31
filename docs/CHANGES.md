# SMMU — changes since the original 1999 tree

A high-level summary of everything done to modernize this SMMU 3.21 checkout, from
"it doesn't build" to a fully playable 64-bit Linux/SDL3 port with sound, music, and
several new features. For the source-backed detail (symptom → root cause → fix →
files) see [`LEGACY_FIXES.md`](LEGACY_FIXES.md).

Starting point: the unmodified 1999 DOS/DJGPP source, plus a partial SDL3 Linux
backend under `linux/` that did not build/run cleanly.

---

## Build & tooling (`Makefile.sdl3`)

- Fixed the link: `version.o` was missing from `OBJS` (undefined `VERSION`).
- Fixed `make debug` (it fell through to the dead DOS `makefile`; now `-f Makefile.sdl3`).
- Added a `run` target so every build drops the binary + `smmu.wad` symlink into `run/`.
- Added the load-bearing compiler flags a from-scratch build needs on modern gcc:
  - **`-fcommon`** — the 1999 tree has tentative-definition globals that `-fno-common`
    (today's default) rejects as multiple-definition link errors.
  - **`-fno-strict-aliasing`** — the engine type-puns; `-O2` miscompiles otherwise.
  - **`-MMD -MP`** header-dependency tracking — editing a `.h` now recompiles its
    dependents (no more `make clean`).
- Initialized a git repo with a `.gitignore` that excludes build artifacts,
  generated files, and the copyrighted id IWADs.

## Portability / correctness fixes

- **64-bit startup crash** — `M_LoadDefaults` truncated string config-default pointers
  via `(int) strdup(...)`, corrupting `wad_files[]` etc. → SIGSEGV in
  `D_ProcessWadPreincludes`. Now stores the full pointer.
- **"Everything is dark"** — `I_SetPalette` shifted gamma values `>> 2` (VGA 6-bit DAC),
  making the SDL 8-bit output ¼ brightness. Fixed to full range.
- **Renderer overflow guard** — the static `openings[]` array had no bounds check;
  a pathological view could silently corrupt memory. Now `I_Error`s instead.
- **WiggleHack II** — tall walls no longer shimmer/wiggle as you move (per-wall
  fixed-point precision + scale clamp, ported from Woof; `r_segs.c`/`r_main.c`).
- **Long-wall wobble fix** — wall textures no longer shear/mis-align on long walls
  in large maps (precise per-seg length/angle at load + int64 distance/offset math;
  `p_setup.c`/`r_segs.c`).
- **Tall (DeePsea, >254-row) textures** — modern limit-removing PWADs' tall
  textures (e.g. Legacy of Rust `ZZZGATE*`) no longer render as scrambled bands;
  a flat opaque composite is built for 1s walls and a cumulative-topdelta posted
  composite for 2s mid-textures (`R_GetColumn`/`R_GetColumnMasked`, `r_data.c`).
  Stock textures render identically.
- **Renderer-correctness batch** (Woof parity) — overflow-safe BSP clip angle
  (`R_PointToAngleCrispy`, fixes seg flicker/HOM on huge maps), 64-bit sprite
  clipping (`sprtopscreen`) + tall-sprite posts, 64-bit sprite projection
  (`FixedMul64`, fixes edge flicker), `finetangent[]` mask, release-safe sprite
  column clamp, stable equal-distance sprite sort, and a `TEXTURE1` bounds check.
- **Settings now save on exit** — `atexit(I_Quit)` was never registered, so config
  was never written; also window-close now quits+saves. Config lives next to the
  binary (`run/smmu.cfg`).
- **Screen-size view overflow** — blocks ≥ 11 are clamped to fullscreen in one place
  (`R_ExecuteSetViewSize`), fixing a release-only `R_RenderSegLoop` crash.

## Sound (new)

- **SFX** — implemented a software mixer in `linux/i_sound.c`: DMX lump parse,
  resample to 44.1 kHz, x² stereo pan, up to 128 voices, into an SDL3 pull callback.
  (Also fixed `snd_card = 0`, which had made the `S_*` layer drop every sound.)
- **Music** — authentic **OPL3 (Adlib) synthesis**, no external soundfont: ported
  Nuked-OPL3 (`opl3.c`), a GENMIDI voice player (`i_opl.c`), and a MUS/MIDI sequencer
  (`i_mus.c`); wired into the audio callback and mixed over the SFX. (Also fixed that
  `I_InitMusic` was never called — now invoked from `S_Init`.)

## Video / input

- **Hi-res by default** — `hires = 1` (true 640×400 internal framebuffer); the SDL3
  backend is hires-aware. Lowres mode and its "video mode" menu toggle were removed.
- **Mouse grab** — the pointer is released (cursor back) in menus/console/pause and
  only captured during active gameplay.
- **Window sizing from the command line** — `-2` / `-3` / `-4` scale the window ×2/×3/×4
  and `-geom WxH` sets an explicit size (SDL nearest-neighbour stretches the 640×400
  framebuffer). These revive the old X11-backend flags on top of `SMMU_SCALE`. (The
  other legacy flags — `-disp`, `-noaccel`, `-grabmouse` — were intentionally left out;
  SDL/the auto-grab already cover them.)

## HUD (new / reworked)

One unified `screensize` control now runs the whole progression:

| screensize | display |
|---|---|
| 0–7 | windowed 3D view + status bar |
| 8 | fullscreen + classic text overlay |
| 9 | fullscreen + **GZDoom-style graphical HUD** |
| 10 | the same HUD at **50%** |
| 11 | the **vanilla status bar scaled to 50%**, centred |

## Automap

- **Textured automap** (`automap_textured`, on by default; Options → automap →
  "textured display") — explored subsectors are filled with their floor flats,
  light-shaded.

## Menus

- **Key bindings menu** — SMMU 3.21 shipped a dead, non-selectable "key bindings"
  entry. Built a working `menu_keybindings` with a "press a key" capture widget;
  bindings persist to the config.

## Persistence

- All settings (screen size, HUD, key bindings, automap options, gamma, volumes, …)
  save to `~/.smmu/smmu.cfg` on exit. **Window size is not persisted** (it derives
  from `SMMU_SCALE` each launch).

## Documentation

- [`docs/LEGACY_FIXES.md`](LEGACY_FIXES.md) — the detailed fix log + an audit of this
  tree against BuddyDoom's fix list (what SMMU already carries natively from MBF,
  what's deliberately kept for demo compat, what's deferred).
- [`docs/CHANGES.md`](CHANGES.md) — this file.
- `CLAUDE.md` stays in the repo root (Claude Code auto-loads it from there) and points
  here for the full detail.

---

## Command-line quick reference

```
./smmu -iwad DOOM2.WAD                 # required: which IWAD
./smmu -iwad DOOM2.WAD -file MY.wad    # load a PWAD (note: -file, not -wad)
./smmu -iwad doom1.wad -warp 1         # jump to a map
SMMU_SCALE=2 ./smmu -iwad DOOM2.WAD    # 2x window (1280x800)
```

Music/SFX are on by default; `-nosound` / `-nomusic` disable them.
