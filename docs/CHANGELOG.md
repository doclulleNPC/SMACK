# Changelog

Every change since the unmodified SMMU 3.21 (1999) source, newest first.

This is the chronological record. For a grouped, thematic overview of what the
fork adds see [`CHANGES.md`](CHANGES.md); for the source-backed detail on
individual portability fixes (symptom → root cause → fix → files) see
[`LEGACY_FIXES.md`](LEGACY_FIXES.md).

The starting point was the 1999 DOS/DJGPP tree plus a partial, non-building
SDL3 Linux backend under `linux/`.

---

## 2026-08-13

### Automatic version bumping

- The fork now carries its own version — `smack_version` in `version.c`, shown in
  the startup banner — independent of the SMMU 3.21 base.
- `tools/hooks/pre-commit` bumps it on every commit and stages the bump into that
  same commit: **+0.0.1** for a small change, **+0.1.0** for a larger feature.
  Chosen by, in order, a `SMACK_BUMP` override, a newly added `.c`/`.h` file, or
  the staged line count (≥ 200 by default). Merges, rebases, cherry-picks and
  reverts are skipped.
- `tools/bump-version.sh` does the edit and is usable by hand; POSIX sh with no
  `sed -i`, so Linux, macOS and Windows behave identically.
- Hooks live in `tools/hooks` (which *is* version-controlled, unlike `.git/hooks`);
  `sh tools/install-hooks.sh` points `core.hooksPath` at them.

### Unbinding a key (menu)

- **Delete** clears a binding in Options → key bindings; it then shows `---`.
  Previously the widget could only assign or cancel, and there was no console
  route either, so unbinding meant hand-editing `ID0/smack.cfg`.

## 2026-08-12

### Application icon

- `res/smack.ico` (16/24/32/48/64/128/256, 32-bit with a real AND mask) is linked
  into the Windows builds as a resource, so `smack.exe` is branded in Explorer, the
  taskbar and Alt-Tab — mingw via `windres`, MSVC via `rc`, and the VS project via a
  `ResourceCompile` item.
- The **window** icon is set at runtime from `res/icon_rgba.h` via
  `SDL_SetWindowIcon`, which is what gives Linux an icon at all (there is no
  resource section to read one from) and covers the title bar on both platforms.
- `res/smack.desktop` + `res/smack.png` for Linux desktop-menu integration.
- Both artefacts are generated from `tools/appicon.png` by `tools/make-icon.ps1`
  and committed, so an ordinary build needs no image tooling.

### Config saving, dithered lighting, weapon autoswitch, gamepad (`cc168a5`)

- **Settings are saved again.** They never were, and the file location was a red
  herring: the game **segfaulted during shutdown**, part way through
  `M_SaveDefaults`, so the `rename()` that installs the new config never ran —
  leaving a truncated `tmpsmack.cfg` behind. `default_t.defaultvalue` and
  `orig_default` had been widened to `intptr_t` for 64-bit, but the local
  `value` in `M_SaveDefaults` was still an `int`, so a `dt_string` default
  truncated a `char *` to 32 bits and the `fprintf("%s")` dereferenced it. It
  died on the first string entry (`name`), 406 bytes in. String locations are
  now also read as `*(char **)dp->location`.
- The config **temp file** moved to the `ID0` data directory, beside the file it
  replaces.
- **Settings are written when a menu closes** (`MN_ClearMenus`), not only on a
  clean exit, so menu changes survive a crash or a killed process.
- **Dithered lighting** — Options → video → rendering → "dither light levels"
  (`r_dither`, default on). Doom picks one of 32 light bands, which shows as
  hard seams in shadows. The light index is now computed with two extra bits of
  precision and an ordered dither steps some pixels to the next band: across
  screen columns for walls, across rows for floors and ceilings, matching the
  axis the banding runs along in each case.
- **Weapon autoswitch toggle** — Options → weapons → "switch on pickup"
  (`weapon_autoswitch`, default on = vanilla). Honoured only in ordinary single
  player; netgames and demos always use vanilla behaviour so nothing desyncs.
- **Gamepad support.** The engine already had the whole joystick path
  (`joyxmove`/`joyymove`/`joybuttons`, the `ev_joystick` case in `G_Responder`,
  the `joyb_*` bindings, the "enable joystick" menu toggle) but no platform code
  ever opened a device, so the option did nothing. `linux/i_video.c` now opens
  the first gamepad, handles hotplug, and posts one `ev_joystick` per tic.
  Sticks report ‑1/0/+1 outside a deadzone because `G_BuildTiccmd` only tests
  them for sign. `-nojoy` skips it.

### Renderer and input fixes (`62a10bb`)

- **Garbage columns on two-sided middle textures** (fences, grates, bars) fixed.
  `R_GetColumnMasked` kept vanilla's shortcut of reading the raw patch lump for
  single-patched columns, but that pairing is no longer valid here:
  `R_GenerateLookup` composites *every* column and assigns
  `texturecolumnofs[x]` unconditionally as a composite offset, while
  `texturecolumnlump[x]` still names the source patch. The raw-patch path
  therefore indexed a patch lump with a composite offset, and
  `R_RenderMaskedSegRange` interpreted the arbitrary bytes as a posted column —
  random topdeltas and lengths drawn as speckled vertical stripes running past
  the floor. One-sided walls were unaffected (separate `texturecolumnofs2[]`).
- **Holding a mouse button fired only once.** `G_Responder` treats `ev_mouse` as
  state, so every event must carry the full button mask; `post_mouse_motion`
  hardcoded `data1 = 0` (so any movement released every button) and
  `post_mouse_button` ignored its `down` argument (so a release looked like a
  press).

### Build batch files, `ID0` data directory, Steam IWAD search (`ddbe316`)

- **`build-mingw.bat` / `build-vs2019.bat`** — wrappers that locate the
  toolchain and hand off to the makefiles.
- **`tools/check-sources.ps1`** — compares the source lists in the three
  makefiles and the vcxproj, which nothing previously kept in step.
- **`ID0` data directory.** `D_DoomDataDir()` returns `<exe dir>/ID0` and
  creates it; WADs, `smack.cfg` and savegames live there, leaving `run/` to hold
  just the binary and its libraries. `smack.wad` is still accepted next to the
  binary, and `-file` resolves a name as given before trying `ID0`. Savegames
  previously defaulted to `"."` — wherever you happened to launch from.
- **Steam IWAD search** (`d_iwad.c`, new). Finds Steam from the registry on
  Windows and the usual locations on Linux/macOS (including Flatpak), reads
  `libraryfolders.vdf` for other drives, and searches the classic, BFG and 2024
  re-release layouts. Runs last, so an IWAD in `ID0` always wins. Names are
  tried as-given, lowercased and uppercased, since Steam ships `DOOM.WAD` in
  capitals while the engine's own lookup is lowercase-only. The re-release
  directories are searched last because their ID24 IWADs crash `R_Init`.

### One `run/` directory; SMACK naming (`e335250`)

- Four runtime directories (`run`, `run-win`, `run-msvc`, `run-msvc-static`)
  collapsed into one `run/`; every build deploys there.
- `run/smmu.bat` → `run/smack.bat`, rewritten (the original launched the DOS
  build from a hard-coded `C:\doom2`).
- `run/README.txt` rewritten — it was Linux-only and stale on nearly every point
  (claimed sound played silence, config in `~/.smmu/`, a 320×200 default).
  *(Later moved to `docs/RUNNING.md`.)*
- Build files and doc titles now say SMACK where they mean this project.

### Standalone single-file Windows build (`66763a1`)

- `nmake /f Makefile.msvc STATIC=1`, or the **ReleaseStatic** configuration,
  links both the CRT (`/MT`) and SDL3 into the executable: no `SDL3.dll`, no
  VC++ redistributable, only Windows' own system DLLs.
- `msvc\build-sdl3-static.bat` fetches the matching SDL source and builds a
  static SDL3, since the `SDL3-devel-VC` SDK ships only a DLL and an import
  library.

### Visual Studio 2019 (MSVC) build (`68c7fb0`)

- `Makefile.msvc` (NMAKE) and `msvc\SMACK.sln`.
- `msvc\compat\` supplies what MSVC lacks: `unistd.h` and `sys/time.h` stubs,
  plus a force-included header for `PATH_MAX`, `strcasecmp` and `S_ISDIR`.
- Neither gcc flag has an MSVC counterpart: MSVC already merges
  tentative-definition globals (no `-fcommon`) and does no type-based alias
  analysis (no `-fno-strict-aliasing`).
- **Packed-struct fix.** `doomdef.h` defines `__attribute__` away on non-gcc
  compilers, silently unpacking `__attribute__((packed))` structs. Three of them
  are *file* layouts, not just memory layouts — `animdef_t` (the ANIMATED lump,
  23 bytes) and the two BMP screenshot headers (14/40) — and MSVC padded all
  three. Explicit `#pragma pack(push,1)` guards added; verified both compilers
  now agree.
- Portability fixes needed by MSVC, all behaviour-preserving: `default:`
  immediately before `}` needs a statement; `{}` is a GCC empty-initializer
  extension; arithmetic on the `void *` from `W_CacheLumpNum` is a GCC
  extension.

## 2026-08-11

### Native Windows (mingw-w64 + SDL3) build (`0148049`, `eed7839`)

- `Makefile.mingw` produces a native Win64 `.exe` with no Cygwin/MSYS runtime
  dependency, linking directly against `SDL3.dll` so the MSVC SDL3 package works
  without an import library.
- **`z_zone.c`/`.h` normalized to lowercase real files.** They were uppercase
  `Z_ZONE.C`/`.H` plus lowercase symlinks; on a case-insensitive filesystem
  those are *one* directory entry, so each symlink pointed at itself and the
  zone allocator source vanished from the working tree.
- **`<values.h>` dropped** — neither mingw-w64 nor Cygwin ships it. Only
  `MAXINT`/`MININT`/`MAXSHORT` were used and glibc defines those from
  `<limits.h>` anyway, so they now come from `<limits.h>` unconditionally.
- One-argument `_mkdir` shim for the Windows CRT.
- Binary file I/O needed no changes — the MBF code's DOS heritage means
  `w_wad.c` already uses `O_BINARY` and the save/demo/tranmap paths use
  `"rb"`/`"wb"`.
- `CLAUDE.md` refreshed and the Windows build documented.

## 2026-07-31

### Project renamed to SMACK! (`89569cc`, `d173503`)

- Renamed from SMMU to **SMACK!**; added the top-level `README.md`. Full binary
  rename `smmu` → `smack`, including the PWAD and config names, which the engine
  derives from `argv[0]`.

### Renderer correctness (`184aff9`, `a83f254`, `006023c`)

Ported from Woof! and its lineage, all demo/netgame-safe:

- **WiggleHack II** — fixes the tall-wall texture shimmer.
- **Tall (DeePsea, >254-row) texture support.**
- **Long-wall texture wobble** — precise seg length/angle with 64-bit
  intermediates.
- Overflow-safe BSP clipping and 64-bit sprite clipping/projection.

## 2026-07-25

### Window sizing and command-line docs (`4a49587`)

- `-2` / `-3` / `-4` and `-geom WxH` window-sizing parameters, on top of the
  `SMACK_SCALE` environment variable.
- `docs/PARAMETERS.md`, enumerated from the `M_CheckParm` calls in the source.

### Settings persistence (`f16b345`)

- The original never wrote its config: `atexit` registered an empty
  `I_Shutdown` stub rather than `I_Quit`, so `M_SaveDefaults` was never called.
  *(A second, deeper bug in the same area — the 64-bit truncation crash — was
  only found and fixed on 2026-08-12, above.)*

### OPL3 music (`1500546`)

- Authentic **OPL3 (Adlib) synthesis**: Nuked-OPL3 (`opl3.c`) plus a GENMIDI
  voice player (`i_opl.c`) and a MUS/MIDI sequencer (`i_mus.c`), rendered into
  the SDL3 audio callback and mixed over the sound effects. No soundfont or
  external synth needed.

### Modernized for 64-bit Linux + SDL3 (`1fadf9e`)

The initial pass that took the 1999 tree from "does not build" to playable:

- Fixed the build: missing `version.o`, `make debug` falling through to the dead
  DOS makefile, and the load-bearing flags a from-scratch build needs on modern
  gcc — **`-fcommon`** for the era's tentative-definition globals and
  **`-fno-strict-aliasing`** because the engine type-puns and `-O2` miscompiles
  it otherwise.
- 64-bit portability fixes throughout.
- Working SDL3 video and sound-effect backends under `linux/`.
- Hi-res 640×400 rendering, the HUD/screen-size rework, textured automap, and a
  working key-bindings menu (the original's was a dead link).
- **Palette fix**: `I_SetPalette` must not shift gamma values `>> 2` — that was
  for the VGA 6-bit DAC and made SDL's 8-bit output quarter-brightness.

---

## Inherited from SMMU 3.21

Not part of this fork, but worth knowing what the base already provides: Boom
and MBF features, hub levels, player skins, an in-game console, and
FraggleScript. Because SMMU is MBF-derived it also already carries Lee
Killough's 1998 64-bit and limit-removing cleanups — see `LEGACY_FIXES.md` §9.
