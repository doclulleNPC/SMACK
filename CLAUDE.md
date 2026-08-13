# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**SMACK!** — a fork of SMMU 3.21 ("Smack My Marine Up"), the 1999 MBF-based Doom
source port by Simon Howard ("Fraggle"). The C source in the repository root is
the original DOS/DJGPP codebase; the active work here is **modernizing it to
build and run on 64-bit Linux with an SDL3 backend**, plus sound/music and
renderer fixes. Beyond stock Doom it carries Boom + MBF features, hub levels,
player skins, an in-game console, and **FraggleScript** (a level-scripting
language, the `t_*.c` files).

The binary, its PWAD, and its config are all named `smack` (`smack.wad`,
`smack.cfg`) — the rename from `smmu` is complete, but much prose in
`docs/`, `changes.txt`, and the `smmu*.txt` files still says "SMMU". Read
`README.md` for the user-facing summary.

Three docs are worth knowing (all under `docs/`):
- **`LEGACY_FIXES.md`** — source-backed log of every portability/modernization
  fix (symptom → root cause → fix → files), plus an audit of what SMMU already
  inherits from MBF. Check here before "fixing" something legacy-looking.
- **`CHANGES.md`** — feature-level summary of everything done since the 1999 tree.
- **`PARAMETERS.md`** — the authoritative command-line reference, enumerated from
  the `M_CheckParm` calls in the source.

## Building

The modern build uses `Makefile.sdl3` (NOT the root `makefile`, which is the
original DJGPP/DOS build and is kept only for reference):

```bash
make -f Makefile.sdl3          # release build -> obj/smack, then copied into run/
make -f Makefile.sdl3 debug    # debug build (-O0 -DRANGECHECK -DINSTRUMENTED) -> objdebug/smack
make -f Makefile.sdl3 clean     # remove obj/ and objdebug/
```

Requires `gcc`, `pkg-config`, and SDL3 dev libs (`pkg-config sdl3` must resolve).
There is no test suite and no linter — verification is compile + run. **Check
`pkg-config --exists sdl3` before promising a build**: this checkout is sometimes
mounted on a machine without SDL3 (e.g. a Windows/MSYS host), where the build
cannot be run at all and edits must be verified by reading.

The default target is `all: $(EXE) run` — every build also **copies the binary to
`run/smack` and symlinks `run/smack.wad`**. `debug` just re-invokes `all` with
`MODE=DEBUG`, so *a debug build overwrites `run/smack` with the debug binary*;
re-run the release build to put the optimized one back. Everything in `run/`
except `README.txt` and `smmu.bat` is generated or user-supplied and gitignored
(binary, WAD symlinks, IWADs, `*.cfg`).

`Makefile.sdl3` carries two load-bearing flags: **`-fcommon`** (the 1999 source has
tentative-definition globals that modern gcc's `-fno-common` rejects on a from-scratch
link) and **`-fno-strict-aliasing`** (the engine type-puns; `-O2` miscompiles it
otherwise). It also uses `-MMD -MP` for header dependencies, so editing a `.h`
recompiles its dependents.

New `.c` files must be added to `OBJS` by hand — there is no wildcard. Root-level
and `linux/` sources share one object directory via two pattern rules, so a
basename may not be used twice across those directories. **`Makefile.sdl3` and
`Makefile.mingw` each carry their own copy of `OBJS`** — add new sources to both.

### Versioning

The fork's own version is `const char smack_version[]` in `version.c` (the
SMMU `VERSION = 321` beside it is the base engine's and does not move). It shows
in the startup banner.

**It is bumped automatically on every commit** by `tools/hooks/pre-commit`, which
stages the change into that same commit. Install once per clone:

```sh
sh tools/install-hooks.sh     # sets core.hooksPath = tools/hooks
```

How the level is chosen, in order: `SMACK_BUMP=none|patch|minor|major` if set →
a newly added `.c`/`.h` file (a feature by definition) → total staged
added+deleted lines ≥ `SMACK_BUMP_MINOR_LINES` (default 200) → otherwise patch.
`SMACK_NO_BUMP=1` skips it. Merges, rebases, cherry-picks and reverts are left
alone so replayed work doesn't fight over `version.c`.

The size heuristic is a proxy, not a judgement — **force it when you know
better**, which is the common case for a feature that happens to be a small
diff:

```sh
SMACK_BUMP=minor git commit -m "..."
```

`tools/bump-version.sh show|patch|minor|major|set X.Y.Z` does the edit and is
fine to run by hand. Both scripts are POSIX sh and avoid `sed -i` (GNU and BSD
disagree about its argument), so they behave identically on Linux and on Windows
under Git's bundled sh.

One local gotcha: this repo has `core.autocrlf=true` and some files are stored
with CRLF, so an editor that rewrites a file as LF makes the whole file show as
changed. That inflates the line count the heuristic reads — check
`git diff --cached --numstat` if a bump level surprises you.

### Batch wrappers (the easy path on Windows)

`build-mingw.bat` and `build-vs2019.bat` at the repo root locate the toolchain
and then hand off to the makefiles, so they cannot drift from the real build:

```
build-mingw.bat  [debug|clean|rebuild]
build-vs2019.bat [debug|static|ide|clean|rebuild]
```

Both run `tools\check-sources.ps1` first, which compares the source lists in
`Makefile.sdl3`, `Makefile.mingw`, `Makefile.msvc` and `msvc\SMACK.vcxproj` and
warns if they disagree — **there are four separate lists**, and forgetting one
produces a link error on a toolchain you weren't using at the time. Run it by
hand after adding a `.c` file; the vcxproj is generated from `Makefile.msvc`.

Two batch-scripting traps bit this repo twice each, so watch for them:
`%ProgramFiles(x86)%` expanded with `%` inside a parenthesised block or `for`
list closes the block at its literal `(x86)` — use delayed expansion (`!VAR!`).
And a bare `smack.exe` fails even from its own directory when
`NoDefaultCurrentDirectoryInExePath` is set (MSYS/Git-Bash shells export it) —
invoke by full path.

### Windows build

`Makefile.mingw` produces a native Win64 `.exe` (no Cygwin/MSYS runtime
dependency) from the same sources:

```bash
make -f Makefile.mingw          # -> obj-win/smack.exe, copied into run/
make -f Makefile.mingw debug    # -> objdebug-win/smack.exe
make -f Makefile.mingw clean
```

It needs a mingw-w64 gcc and an SDL3 SDK, both overridable:
`make -f Makefile.mingw CC=x86_64-w64-mingw32-gcc SDL3_DIR=C:/Source/SDL3`.
The **MSVC** SDL3 package (`SDL3-devel-VC`) is fine — the link is made directly
against `lib/x64/SDL3.dll`, so GNU ld synthesizes the import stubs and no
mingw import library is needed. Pass `SDL3_DIR` as a Windows-style path
(`C:/...`): the build is usually driven from MSYS/Git-Bash while the compiler is
a Cygwin program, and the two disagree about what `/c/...` means.

`run/` is the shared runtime image (`smack.exe` + `SDL3.dll` +
`smack.wad`); it is populated by copying rather than symlinking. All three files
must sit together because `D_DoomExeDir()` derives the data directory from
`argv[0]`.

### Windows build with Visual Studio 2019 (MSVC)

A second Windows toolchain, independent of the mingw one:

```
nmake /f Makefile.msvc            # -> obj-msvc\smack.exe, copied into run\
nmake /f Makefile.msvc CFG=Debug  # -> obj-msvc-debug\
nmake /f Makefile.msvc clean
```

Run it from a VS2019 x64 developer prompt (or after `vcvars64.bat`); NMAKE, not
GNU make. `msvc\SMACK.sln` is the IDE equivalent — same switches, output in
`obj-msvc-ide\` — and both populate `run\`. Override the SDK with
`SDL3_DIR=...` (nmake) or an `SDL3_DIR` environment variable (IDE). This build
wants the **MSVC** SDL3 package, since it links `SDL3.lib` properly rather than
going straight at the DLL like the mingw build does.

**`Makefile.msvc` and `msvc\SMACK.vcxproj` each carry their own source list**, as
does each gcc makefile — four lists to update when adding a `.c` file. The
vcxproj is generated from `Makefile.msvc`'s `OBJS`, so regenerate rather than
hand-edit if you add many files at once.

**Standalone single-file build:** `nmake /f Makefile.msvc STATIC=1` (or the
**ReleaseStatic** configuration in the IDE) links both the CRT (`/MT`) and SDL3
into the exe, so it imports only Windows' own system DLLs — no SDL3.dll, no
VC++ redistributable. It needs a static SDL3, which the SDL3-devel-VC SDK does
not ship; `msvc\build-sdl3-static.bat` fetches the matching SDL source and
builds one into `C:\Source\SDL3-static` (override with `SDL3_STATIC_DIR`).
Things that matter here:
- Static and shared objects **must not be mixed in one link** (`/MT` vs `/MD`
  means two separate CRT heaps), so `STATIC=1` compiles into its own
  `obj-msvc-static\`. Keep those object directories separate. They do share the
  one `run\` output directory, though, so a static and a shared build overwrite
  each other's `smack.exe` there — as do the mingw and MSVC builds.
- SDL must be built with the **same** CRT model — hence the script's
  `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` plus
  `-DCMAKE_POLICY_DEFAULT_CMP0091=NEW` (without the policy, the setting is
  silently ignored).
- The Win32 libraries the static link needs are SDL's, not ours: the list comes
  from `Libs.private` in `$(SDL3_STATIC_DIR)\lib\pkgconfig\sdl3.pc`. Re-read it
  there if a future SDL version fails to link rather than guessing.

MSVC specifics worth knowing:
- **`msvc\compat\`** supplies what MSVC lacks: `unistd.h` and `sys/time.h`
  stubs (on the include path for this build only), plus `msvc_compat.h`, which
  is **force-included into every file** (`/FI`) for `PATH_MAX`, `strcasecmp` and
  `S_ISDIR`. Keep it small — per-file needs belong in that file.
- **No `-fcommon` equivalent is needed**: MSVC already merges the 1999 tree's
  tentative-definition globals, so they link without complaint.
- **No `-fno-strict-aliasing` equivalent is needed** either — MSVC does not do
  type-based alias analysis, so the engine's type punning is safe at `/O2`.
- **NMAKE has no header dependency tracking** (no `-MMD -MP`). Editing an engine
  header does not rebuild its dependents; run `clean` first. Only the compat
  headers are wired up explicitly.
- `__attribute__` is defined away for non-gcc compilers in `doomdef.h` /
  `m_fixed.h`, which quietly **unpacks `__attribute__((packed))` structs**. The
  three that are read from or written to files — `animdef_t` (the ANIMATED lump,
  23 bytes) in `p_spec.c` and the two BMP screenshot headers (14/40 bytes) in
  `m_misc.c` — therefore carry explicit `#pragma pack(push,1)` guards. Any new
  packed struct needs the same treatment or it will silently gain padding.

Windows-specific gotchas when testing:
- **`timeout N ./smack.exe` does not kill it.** Cygwin's `timeout` sends SIGTERM,
  which a native Windows process ignores; `timeout` reports 124 while the game
  keeps running. Use `Stop-Process -Name smack -Force`, and check for strays
  before trusting an observation.
- **Redirected stdout is block-buffered**, so a log that appears to stop at
  `D_SetGraphicsMode` usually means the game is running fine with the rest of the
  banner still in the buffer — confirm with `Get-Process smack` (window title,
  CPU) rather than by reading the log.
- The SDL **dummy video driver** is not a good smoke test here; prefer inspecting
  the real window.

Key compile flags (see `Makefile.sdl3`): `-DSDL3` selects the SDL backend,
`-DDOGS` enables the helper-dogs feature, and several `-Wno-*` flags suppress
warnings from legacy K&R-era pointer/qualifier code. Do not "fix" those warnings
wholesale; they are silenced deliberately.

## Running

The binary resolves its data directory from `argv[0]` via `D_DoomExeDir()`, so
it must run alongside its WADs. Use the prepared `run/` directory:

```bash
cd run && ./smack -iwad DOOM2.WAD             # smack.wad (the port's PWAD) is symlinked here by the build
cd run && ./smack -iwad doom1.wad -warp 1     # shareware IWAD, jump to MAP01
cd run && ./smack -iwad DOOM2.WAD -file X.wad # load a PWAD — the flag is -file; there is no -wad
```

An IWAD (`DOOM.WAD`, `DOOM2.WAD`, `doom1.wad`) is required on first run; none is
committed (id copyright). `docs/PARAMETERS.md` is the full flag list and
`docs/RUNNING.md` is the player-facing guide (files, IWAD search order, display /
sound / input options); it used to live at `run/README.txt`. `docs/CHANGELOG.md`
records every change since vanilla SMMU in date order. Useful environment/config knobs:

- Rendering is always hi-res 640x400 (`hires=1` in `linux/i_video.c`; lowres support and its video-mode menu toggle have been removed, though the renderer still keys off `SCREENWIDTH<<hires`). Window size is decided at startup in `linux/i_video.c` by, in order of precedence, **`-geom WxH` → `-4`/`-3`/`-2` → `SMACK_SCALE=N` → 1x** (a 640x400 window); the scale magnifies the window on top of the fixed render framebuffer.
- The HUD is driven by one `screensize` control (cvar `screensize`, range 0–11; the menu "screen size" slider). 0–7 = windowed 3D view + status bar; **8 = fullscreen + classic text overlay**, **9 = fullscreen + GZDoom-style graphical HUD**, **10 = the same HUD at 50%**, **11 = the vanilla status bar scaled to 50%, centred (aidoom-style, `ST_DrawScaled` in `st_stuff.c`)**. Blocks ≥ 11 are all clamped to fullscreen view in `R_ExecuteSetViewSize` (the single clamp point — every `R_SetViewSize(screenSize+3)` call site relies on it). The graphical HUD is `HU_DrawFullHUD` in `hu_over.c` (dispatched from `HU_OverlayDraw` on `screenSize`); its 50% variant reuses the full-size draw code but swaps `V_DrawPatch` (2× hires) for `V_DrawPatchUnscaled` (1× native). `hud_overlaystyle`/cvar `hu_overlay` (HUD settings → "display type") now only selects the text-overlay styles 0–3 used at screensize 8.
- Options → **key bindings** opens `menu_keybindings` (`mn_menus.c`), built at startup by `MN_InitKeyBindings`. Selecting a row runs `mn_bindkey N`, which installs `binding_widget` (a `menuwidget_t` capture prompt); the next keypress is written to the corresponding `key_*` variable (ESC cancels). The menu engine has no dedicated key-binding item type, so this is done with the `current_menuwidget` mechanism (same pattern as `mn_misc.c`'s `popup_widget`).
- **Textured automap:** `AM_drawFlats` in `am_map.c` (cvar `automap_textured`, default on; menu: Options → automap → "textured display") fills each explored subsector's floor area with its floor flat, light-shaded — sampling per pixel over BLK×BLK blocks (one `R_PointInSubsector` BSP descent per block). Uses `firstflat + flattranslation[pic]` for the flat lump and `colormaps[0] + cm*256` for shading. Cvar registered in `am_color.c`, config default in `m_misc.c` (persists).
- **Config persistence:** all console/config variables (screensize, HUD, key bindings, automap options, gamma, volumes, …) are written to `run/ID0/smack.cfg` when a menu closes (`MN_ClearMenus`) and again on clean exit via `M_SaveDefaults`, called from `I_Quit` (atexit). **Window geometry persists too**: `v_width`/`v_height`/`v_fullscreen` (cvars declared in `linux/i_video.c`) are updated when the window is resized and applied at startup. Command-line sizing (`-geom`, `-2`/`-3`/`-4`, `SMACK_SCALE`) outranks the saved values but is deliberately one-shot — it does not rewrite them, except on a config that has no size yet. The resize handler skips recording while fullscreen, so the monitor size never becomes the windowed size.
- **Palette note:** `I_SetPalette` (`linux/i_video.c`) must NOT shift gamma values `>> 2` — that was for the VGA 6-bit DAC and makes SDL's 8-bit output ¼ brightness (the "everything is dark" bug). It writes full 0–255 values.
- `SDL_VIDEODRIVER=dummy` / `SDL_AUDIODRIVER=dummy` — headless smoke tests; `-nodraw`/`-nosound`/`-nomusic` do the same at the game level.
- **The `ID0` data directory.** `D_DoomDataDir()` in `d_main.c` returns `<exe dir>/ID0` (the name is `DOOMDATADIR`) and creates it on first use. WADs, `smack.cfg` and savegames all live there, so `run/` itself holds only the binary and its DLLs. All four build systems deploy `smack.wad` into `run/ID0/`. Fallbacks are deliberate: `smack.wad` is still accepted next to the binary if it is not in `ID0`, and `-file` resolves a name as given before trying `ID0` (`D_FindUserWad()`), so absolute and relative paths keep working. Savegames used to default to the *current* directory and the config to the binary's directory; both now point at `ID0`.
- **IWAD search order** (`FindIWADFile()` in `d_main.c`): `-iwad` (a file, a directory to search, or a bare custom name) → the `ID0` data directory → the current directory, then the binary's directory → `$DOOMWADDIR`, then `$HOME` → Steam (`d_iwad.c`). Within a directory the standard names are tried in the order `doom2f.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad`, `doom.wad`, `doom1.wad`, so DOOM II wins when several sit side by side. Those names are lowercase and the comparison is a plain `stat()`, so on Linux an uppercase `DOOM2.WAD` is **not** found by the automatic search — only via explicit `-iwad`. (The Steam search in `d_iwad.c` is the exception: it tries each name as-given, lowercased and uppercased, because Steam's classic packaging ships `DOOM.WAD` in capitals.)

Sound is fully implemented. SFX: `linux/i_sound.c` mixes Doom's 8-bit DMX lumps into a 16-bit stereo SDL3 stream via a pull callback (`snd_card` set on init). Music: authentic **OPL3 synthesis** — Nuked-OPL3 (`opl3.c`) + a GENMIDI voice player (`i_opl.c`) + a MUS/MIDI sequencer (`i_mus.c`), rendered into the same audio callback and mixed over the SFX. `I_InitMusic` (called from `S_Init`) loads the IWAD `GENMIDI` and sets `mus_card`. See `docs/LEGACY_FIXES.md` §3/§10.

## Architecture

Classic id-Software Doom layering. Source files use two-letter subsystem
prefixes; learn the prefixes and the codebase becomes navigable:

- **`i_*` — platform/OS layer (the porting boundary).** The only
  platform-specific code. Modern Linux/SDL3 implementations live in **`linux/`**
  (`i_main.c`, `i_video.c`, `i_sound.c`, `i_system.c`, `i_net.c`) and are what
  the SDL3 makefile compiles. `linux/i_xwin.c` and `linux/i_svga.c` are the old
  X11/SVGAlib backends (not built). `djgpp/` holds the original DOS backend
  (Allegro, `.s` asm, MIDI) — reference only. When adding OS features, touch
  `linux/`; everything above the `i_*` layer must stay platform-agnostic.
- **`d_*` — top-level game control.** `d_main.c` owns `D_DoomMain` (the entry
  from `linux/i_main.c`), argument parsing, IWAD detection, and the main loop.
  `d_net.c` is netgame/tick synchronization; `d_deh.c` is DeHackEd/BEX parsing.
- **`g_*` — game logic / player input to game state.** `g_game.c` runs
  gametics, demos, save/load orchestration, skill/level flow.
- **`p_*` — playsim (simulation).** The largest cluster: `p_mobj` (map objects),
  `p_map`/`p_maputl` (movement & collision), `p_enemy` (AI), `p_inter`
  (pickups/damage), `p_spec` (line/sector specials), `p_setup` (level load),
  `p_saveg` (savegames), `p_user`/`p_pspr` (player & weapons). Boom generalized
  linedefs are in `p_genlin.c`; hub levels in `p_hubs.c`; skins in `p_skin.c`.
- **`r_*` — software renderer.** BSP traversal (`r_bsp`), walls (`r_segs`),
  flats/planes (`r_plane`), sprites (`r_things`), column/span drawers (`r_draw`),
  data/texture management (`r_data`). Renders into an 8-bit palettized
  framebuffer that `linux/i_video.c` converts to RGBA and blits via SDL3.
- **`m_*` — utilities & menu engine.** `m_misc` (config, screenshots), `m_random`
  (the deterministic RNG — demo/netgame compat depends on it), `m_cheat`,
  `m_argv`, `m_fixed.h`/`tables.c` (16.16 fixed-point math & trig — the game
  uses no floats in the playsim).
- **`mn_*` — menu system**, **`st_*` — status bar**, **`hu_*` — heads-up/messages**,
  **`wi_*` — intermission**, **`f_*` — finale/screen wipe**, **`am_*` — automap**,
  **`v_*` — low-level video/patch drawing**, **`s_sound` — sound orchestration**
  (calls into `i_sound`), **`w_wad` — WAD file loading**, **`z_zone` — the zone
  memory allocator** that nearly everything allocates through.
- **`c_*` — the in-game console** (`c_io`, `c_runcmd`, `c_cmd`, `c_net`); backtick
  opens it. Console variables/commands are registered across many subsystems —
  see the recipe below. Several subsystems keep their console commands in a
  dedicated file: `g_cmd.c`, `p_cmd.c`, `am_color.c`.
- **`p_info.c` — level info.** SMMU stores per-level metadata (level name, music
  lump, par time, sky, FraggleScript source) *inside the map marker lump*
  (`MAPxx`/`ExMx`), parsed here. This is the MapInfo equivalent and is what ties
  FraggleScript to a level.
- **`opl3.c`/`opl3.h`/`wf_rom.h` are vendored third-party code** (Nuked-OPL3 /
  Nuked-OPL3-fast, LGPL-2.1+) — keep them close to upstream rather than
  restyling them to match the 1999 sources. `i_opl.c` (GENMIDI voice player) and
  `i_mus.c` (MUS/MIDI sequencer) are this fork's own glue and live in the root
  rather than `linux/` because they are platform-agnostic.
- **`r_ripple.c`** — Simon Howard's Quake-style flat warping, used for swirling
  liquid flats.
- **`t_*` — FraggleScript.** `t_script` (script objects), `t_parse`/`t_prepro`
  (parser/preprocessor), `t_func` (built-in functions), `t_oper`, `t_vari`,
  `t_spec`. See `fs_funcs.txt` for the function reference.
- **`info.c`/`info.h`** — the giant sprite/state/mobjinfo tables that drive all
  actor behavior; **`sounds.c`**, **`dogs.c`** (large generated data).

### Cross-cutting things to know

- **Adding a console variable (the standard three-step).** Most user-facing
  settings in this fork are cvars, and each one touches three files:
  1. In the owning subsystem's `.c`, declare the backing global, then use the
     `VARIABLE_*` / `CONSOLE_VARIABLE` (or `CONSOLE_COMMAND`, `CONSOLE_NETVAR`)
     macros from `c_runcmd.h` to build the `variable_t`/`command_t`.
  2. Register it with `C_AddCommand(name)` inside that subsystem's
     `X_AddCommands()` — every one of those is called from `C_AddCommands()` in
     `c_cmd.c` (`Cheat_`, `G_`, `HU_`, `I_`, `net_`, `P_`, `R_`, `S_`, `ST_`,
     `T_`, `V_`, `MN_`, `AM_`). A cvar not registered there simply doesn't exist.
  3. For persistence, add a `default_t` entry to `defaults[]` in `m_misc.c`
     (name, pointer, default, range, type, screen-size class, wad-allowed flag,
     help string). Without it the setting resets every launch.
  Menu exposure is separate again — `mn_menus.c` items reference the cvar by name.
- **Demo/netgame determinism:** the playsim must be bit-for-bit reproducible.
  Anything that affects game state must go through `m_random.c`'s RNG and
  fixed-point math (`m_fixed.h`, `tables.c`), never `rand()` or floats. Renderer
  and platform-layer changes are free; playsim (`p_*`, `g_*`) changes are not.
- **Zone allocator:** prefer `Z_Malloc`/`Z_Free` (`z_zone.h`) with the right tag
  over raw `malloc`; tagged blocks (e.g. `PU_LEVEL`) are freed en masse on level
  change. Crash handlers dump zone history (`Z_DumpHistory`).
- **`z_zone.c`/`z_zone.h` are now ordinary lowercase files.** They used to be
  uppercase `Z_ZONE.C`/`Z_ZONE.H` plus lowercase symlinks (a DOS-filename
  artifact). That layout cannot be checked out on a case-insensitive filesystem:
  `z_zone.c` and `Z_ZONE.C` are one directory entry there, so the symlink pointed
  at itself and the source vanished from the working tree ("Too many levels of
  symbolic links"). Nothing ever included the uppercase spelling, so the pair was
  normalized to lowercase — do not reintroduce the symlinks.
- **Platform-portability edits: prefer a portable formulation over a platform
  `#ifdef`.** `doomtype.h` no longer includes `<values.h>` (absent on both
  mingw-w64 and Cygwin) — `MAXINT`/`MININT`/`MAXSHORT` come from `<limits.h>`
  unconditionally, with the same values glibc's `values.h` gave, and `m_bbox.h`
  picks them up via `doomtype.h`. Where a conditional really is needed, use
  `#if defined(_WIN32) && !defined(__CYGWIN__)`, as the one-argument `_mkdir`
  shim in `d_main.c` does. Binary file I/O was already
  Windows-safe — the MBF code has DOS heritage, so `w_wad.c` uses `O_BINARY` and
  the save/demo/tranmap paths use `"rb"`/`"wb"`.
- Files carry original id/MBF copyright headers and RCS `$Log$` history — this is
  expected; keep the style consistent when editing.
