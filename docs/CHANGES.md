# SMACK! — changes since the original SMMU 1999 tree

A high-level summary of everything done to modernize this SMMU 3.21 checkout, from
"it doesn't build" to a playable 64-bit Linux/SDL3 **and** Windows port with sound,
music, and several new features.

This file is grouped by topic. For the same ground in date order, commit by commit,
see [`CHANGELOG.md`](CHANGELOG.md); for the source-backed detail on individual fixes
(symptom → root cause → fix → files) see [`LEGACY_FIXES.md`](LEGACY_FIXES.md); for
how to actually run the thing see [`RUNNING.md`](RUNNING.md).

Starting point: the unmodified 1999 DOS/DJGPP source, plus a partial SDL3 Linux
backend under `linux/` that did not build/run cleanly.

---

## Build & tooling (`Makefile.sdl3`)

- Fixed the link: `version.o` was missing from `OBJS` (undefined `VERSION`).
- Fixed `make debug` (it fell through to the dead DOS `makefile`; now `-f Makefile.sdl3`).
- Added a `run` target so every build drops the binary + `smack.wad` symlink into `run/`.
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
  binary (`run/smack.cfg`).
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
  framebuffer). These revive the old X11-backend flags on top of `SMACK_SCALE`. (The
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
  save to `run/ID0/smack.cfg`, both when a menu closes and on exit. **Window size is
  not persisted** (it derives from `-geom` / `-2..-4` / `SMACK_SCALE` each launch).
- Saving was broken twice over, and both are fixed: `atexit` originally registered an
  empty stub instead of `I_Quit`, and later a 64-bit truncation in `M_SaveDefaults`
  crashed the writer part way through, so the new config was never installed.
- **`ID0/` data directory** — WADs, config and savegames all live there, leaving
  `run/` to hold just the binary and its libraries. Savegames used to default to the
  current working directory, i.e. wherever you happened to launch from.

## Windows

- **Three toolchains, one runtime directory.** `Makefile.sdl3` (Linux/gcc),
  `Makefile.mingw` (Windows/mingw-w64) and `Makefile.msvc` + `msvc\SMACK.sln`
  (Windows/Visual Studio 2019) all deploy into `run/`.
- **Standalone single-file exe** — `STATIC=1` links both the CRT and SDL3 in, so the
  result imports nothing but Windows' own system DLLs: no `SDL3.dll`, no VC++
  redistributable.
- `build-mingw.bat` and `build-vs2019.bat` locate the toolchain and hand off to the
  makefiles; `tools/check-sources.ps1` guards against the four source lists drifting.
- Portability work this required: the `z_zone` uppercase/symlink pair (uncheckoutable
  on a case-insensitive filesystem), `<values.h>`, packed structs that MSVC silently
  unpacked, and a handful of GCC extensions. See [`LEGACY_FIXES.md`](LEGACY_FIXES.md).

## IWAD discovery

- Searched in order: `-iwad`, the `ID0` data directory, the current directory, the
  binary's directory, `$DOOMWADDIR` / `$HOME`, and finally **Steam**.
- The Steam search finds the install from the registry (Windows) or the usual paths
  (Linux/macOS, Flatpak included), reads `libraryfolders.vdf` so other drives are
  covered, and handles the classic, BFG and 2024 re-release layouts. The re-release
  IWADs are searched last — their ID24 extensions crash this renderer.

## Input

- **Mouse**: holding a button now repeats (motion events used to clear the button
  mask, so fire never refired).
- **Gamepad**: SDL3 gamepad support wired into the joystick path the engine already
  had; enable it with Options → mouse options → "enable joystick".

## Documentation

- [`docs/LEGACY_FIXES.md`](LEGACY_FIXES.md) — the detailed fix log + an audit of this
  tree against BuddyDoom's fix list (what SMMU already carries natively from MBF,
  what's deliberately kept for demo compat, what's deferred).
- [`docs/CHANGES.md`](CHANGES.md) — this file (grouped by topic).
- [`docs/CHANGELOG.md`](CHANGELOG.md) — every change in date order, commit by commit.
- [`docs/RUNNING.md`](RUNNING.md) — the player-facing guide: files, IWAD search,
  display / sound / input options. (Moved here from `run/README.txt`.)
- [`docs/PARAMETERS.md`](PARAMETERS.md) — every command-line switch.
- `CLAUDE.md` stays in the repo root (Claude Code auto-loads it from there) and points
  here for the full detail.

---

## Command-line quick reference

```
./smack -iwad DOOM2.WAD                 # required: which IWAD
./smack -iwad DOOM2.WAD -file MY.wad    # load a PWAD (note: -file, not -wad)
./smack -iwad doom1.wad -warp 1         # jump to a map
SMACK_SCALE=2 ./smack -iwad DOOM2.WAD    # 2x window (1280x800)
```

Music/SFX are on by default; `-nosound` / `-nomusic` disable them.
