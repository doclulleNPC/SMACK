# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

SMMU ("Smack My Marine Up") version 3.21, a 1999 MBF-based Doom source port by
Simon Howard ("Fraggle"). The C source in the repository root is the original
DOS/DJGPP codebase; the active work here is **modernizing it to build and run on
64-bit Linux with an SDL3 backend**. Beyond stock Doom it adds Boom + MBF
features, hub levels, player skins, an in-game console, and **FraggleScript** (a
level-scripting language, the `t_*.c` files).

## Building

The modern build uses `Makefile.sdl3` (NOT the root `makefile`, which is the
original DJGPP/DOS build and is kept only for reference):

```bash
make -f Makefile.sdl3          # release build -> obj/smack
make -f Makefile.sdl3 debug    # debug build (-O0 -DRANGECHECK -DINSTRUMENTED) -> objdebug/smack
make -f Makefile.sdl3 clean     # remove obj/ and objdebug/
```

Requires `gcc`, `pkg-config`, and SDL3 dev libs (`pkg-config sdl3` must resolve).
There is no test suite and no linter — verification is compile + run.

`Makefile.sdl3` carries two load-bearing flags: **`-fcommon`** (the 1999 source has
tentative-definition globals that modern gcc's `-fno-common` rejects on a from-scratch
link) and **`-fno-strict-aliasing`** (the engine type-puns; `-O2` miscompiles it
otherwise). It also uses `-MMD -MP` for header dependencies, so editing a `.h`
recompiles its dependents. **`docs/LEGACY_FIXES.md`** is the full source-backed log of
portability/modernization fixes and an audit of what SMMU already carries from MBF.

Key compile flags (see `Makefile.sdl3`): `-DSDL3` selects the SDL backend,
`-DDOGS` enables the helper-dogs feature, and several `-Wno-*` flags suppress
warnings from legacy K&R-era pointer/qualifier code. Do not "fix" those warnings
wholesale; they are silenced deliberately.

## Running

The binary resolves its data directory from `argv[0]` via `D_DoomExeDir()`, so
it must run alongside its WADs. Use the prepared `run/` directory:

```bash
cd run && ./smack -iwad DOOM2.WAD          # smack.wad (the port's PWAD) is symlinked here
cd run && ./smack -iwad doom1.wad -warp 1  # shareware IWAD, jump to MAP01
```

An IWAD (`DOOM.WAD`, `DOOM2.WAD`, `doom1.wad`) is required on first run. See
`run/README.txt` for the full flag list. Useful environment/config knobs:

- Rendering is always hi-res 640x400 (`hires=1` in `linux/i_video.c`; lowres support and its video-mode menu toggle have been removed, though the renderer still keys off `SCREENWIDTH<<hires`). `SMACK_SCALE=N` magnifies the window on top of the framebuffer (default 1 → a 640x400 window).
- The HUD is driven by one `screensize` control (cvar `screensize`, range 0–11; the menu "screen size" slider). 0–7 = windowed 3D view + status bar; **8 = fullscreen + classic text overlay**, **9 = fullscreen + GZDoom-style graphical HUD**, **10 = the same HUD at 50%**, **11 = the vanilla status bar scaled to 50%, centred (aidoom-style, `ST_DrawScaled` in `st_stuff.c`)**. Blocks ≥ 11 are all clamped to fullscreen view in `R_ExecuteSetViewSize` (the single clamp point — every `R_SetViewSize(screenSize+3)` call site relies on it). The graphical HUD is `HU_DrawFullHUD` in `hu_over.c` (dispatched from `HU_OverlayDraw` on `screenSize`); its 50% variant reuses the full-size draw code but swaps `V_DrawPatch` (2× hires) for `V_DrawPatchUnscaled` (1× native). `hud_overlaystyle`/cvar `hu_overlay` (HUD settings → "display type") now only selects the text-overlay styles 0–3 used at screensize 8.
- Options → **key bindings** opens `menu_keybindings` (`mn_menus.c`), built at startup by `MN_InitKeyBindings`. Selecting a row runs `mn_bindkey N`, which installs `binding_widget` (a `menuwidget_t` capture prompt); the next keypress is written to the corresponding `key_*` variable (ESC cancels). The menu engine has no dedicated key-binding item type, so this is done with the `current_menuwidget` mechanism (same pattern as `mn_misc.c`'s `popup_widget`).
- **Textured automap:** `AM_drawFlats` in `am_map.c` (cvar `automap_textured`, default on; menu: Options → automap → "textured display") fills each explored subsector's floor area with its floor flat, light-shaded — sampling per pixel over BLK×BLK blocks (one `R_PointInSubsector` BSP descent per block). Uses `firstflat + flattranslation[pic]` for the flat lump and `colormaps[0] + cm*256` for shading. Cvar registered in `am_color.c`, config default in `m_misc.c` (persists).
- **Config persistence:** all console/config variables (screensize, HUD, key bindings, automap options, gamma, volumes, …) are written to `run (next to the binary)/smack.cfg` on clean exit via `M_SaveDefaults`, called from `I_Quit` (atexit). The **window size is NOT persisted** — it derives from `SMACK_SCALE` each launch (`win_w`/`win_h` in `linux/i_video.c` aren't config vars).
- **Palette note:** `I_SetPalette` (`linux/i_video.c`) must NOT shift gamma values `>> 2` — that was for the VGA 6-bit DAC and makes SDL's 8-bit output ¼ brightness (the "everything is dark" bug). It writes full 0–255 values.
- `SDL_VIDEODRIVER=dummy` / `SDL_AUDIODRIVER=dummy` — headless smoke tests; `-nodraw`/`-nosound`/`-nomusic` do the same at the game level.
- User config and saves live in `run (next to the binary)/` (`smack.cfg`, `savegames/`).

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
  opens it. Console variables/commands are registered across many subsystems.
- **`t_*` — FraggleScript.** `t_script` (script objects), `t_parse`/`t_prepro`
  (parser/preprocessor), `t_func` (built-in functions), `t_oper`, `t_vari`,
  `t_spec`. See `fs_funcs.txt` for the function reference.
- **`info.c`/`info.h`** — the giant sprite/state/mobjinfo tables that drive all
  actor behavior; **`sounds.c`**, **`dogs.c`** (large generated data).

### Cross-cutting things to know

- **Demo/netgame determinism:** the playsim must be bit-for-bit reproducible.
  Anything that affects game state must go through `m_random.c`'s RNG and
  fixed-point math (`m_fixed.h`, `tables.c`), never `rand()` or floats. Renderer
  and platform-layer changes are free; playsim (`p_*`, `g_*`) changes are not.
- **Zone allocator:** prefer `Z_Malloc`/`Z_Free` (`z_zone.h`) with the right tag
  over raw `malloc`; tagged blocks (e.g. `PU_LEVEL`) are freed en masse on level
  change. Crash handlers dump zone history (`Z_DumpHistory`).
- **`Z_ZONE.C`/`Z_ZONE.H`** are uppercase files with lowercase `z_zone.c`/`.h`
  symlinks (a DOS-filename artifact); edit through either name.
- Files carry original id/MBF copyright headers and RCS `$Log$` history — this is
  expected; keep the style consistent when editing.
