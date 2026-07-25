# SMMU command-line parameters

Every command-line parameter recognized by this SMMU build (enumerated from the
`M_CheckParm` calls in the source). Parameters shown as `-foo VALUE` consume the
following argument; the rest are on/off flags. Music and SFX are **on by default**.

Usage: `./smmu -iwad DOOM2.WAD [options]` (run from the `run/` directory, or wherever
the IWAD and `smmu.wad` live).

---

## WAD / data files

| Parameter | Description |
|---|---|
| `-iwad FILE` | Which IWAD to use (`DOOM.WAD`, `DOOM2.WAD`, `doom1.wad`, …). Required on first run; SMMU auto-detects the game mode from the IWAD. |
| `-file WAD [WAD …]` | Load one or more PWADs on top of the IWAD. (There is **no** `-wad`.) |
| `-deh FILE` / `-bex FILE` | Load a DeHackEd / BEX patch. |
| `-dehout FILE` / `-bexout FILE` | Dump the DeHackEd/BEX processing log to FILE (debugging). |
| `-config FILE` | Use FILE instead of the default `smmu.cfg`. |
| `-save DIR` | Set the savegame directory. |
| `-noload` | Skip the `wadfile_1/2` preload listed in the config (handy when those point at a stale path). |
| `@FILE` | Response file — read additional command-line arguments from FILE. |

## Starting a game

| Parameter | Description |
|---|---|
| `-warp N` (alias `-wart`) | Jump straight into a level: map `N` on DOOM II, or use with `-episode` on DOOM. Implies autostart. |
| `-episode N` | Start on episode `N` (DOOM 1). |
| `-skill N` | Skill level 1–5 (1 = I'm Too Young To Die … 4 = Ultra-Violence, 5 = Nightmare). |
| `-loadgame N` | Load save slot `N` at startup. |

## Gameplay modifiers

| Parameter | Description |
|---|---|
| `-nomonsters` | Start with no monsters. |
| `-respawn` | Monsters respawn after death (as in Nightmare). |
| `-fast` | Fast monsters (quicker movement and attacks). |
| `-turbo N` | Player movement speed as a percentage (e.g. `-turbo 255`). |
| `-dog` | Spawn one MBF helper dog. |
| `-dogs N` | Spawn `N` helper dogs. |
| `-devparm` | Developer mode (extra diagnostics, screenshot key, on-screen timing). |

## Deathmatch / multiplayer

| Parameter | Description |
|---|---|
| `-deathmatch` | Deathmatch mode (1.0 rules). |
| `-altdeath` | Deathmatch 2.0 (weapons and items respawn). |
| `-trideath` | Deathmatch 3.0. |
| `-net` | Start / join a network game. |
| `-frags N` | Frag limit (deathmatch). |
| `-timer N` | Time limit in minutes (deathmatch). |
| `-avg` | "Austin Virtual Gaming" — fixed 20-minute deathmatch timer. |
| `-statcopy PTR` | Copy end-of-level stats to an external address (external stat trackers). |

## Demos

| Parameter | Description |
|---|---|
| `-record FILE` | Record a demo to FILE. |
| `-recordfrom SLOT FILE` | Record a demo starting from savegame `SLOT`. |
| `-playdemo FILE` | Play a demo back at normal speed. |
| `-timedemo FILE` | Play a demo as fast as possible and print a timing/FPS report. |
| `-fastdemo FILE` | Play a demo at maximum speed (uncapped). |
| `-maxdemo N` | Demo recording buffer size, in KB. |

## Sound

| Parameter | Description |
|---|---|
| `-nosound` | Disable all audio (SFX and music). |
| `-nosfx` | Disable sound effects only. |
| `-nomusic` | Disable music only. |

## Video / display

| Parameter | Description |
|---|---|
| `-nodraw` | Don't render the 3D view (server-style / smoke test). |
| `-noblit` | Render but don't blit to the screen. |
| `-nomouse` | Disable mouse input. |
| `-2` / `-3` / `-4` | Window scale ×2 / ×3 / ×4 (1280×800 / 1920×1200 / 2560×1600). Overrides `SMMU_SCALE`. |
| `-geom WxH` | Create the window at exactly `W`×`H` pixels; the 640×400 framebuffer is nearest-neighbour stretched to fit (e.g. `-geom 1280x800`). Overrides the scale flags. |

**Legacy — inert in this SDL3 build** (they belong to the old X11/SVGAlib backends in
`linux/i_xwin.c` / `linux/i_svga.c`, which are not compiled): `-disp NAME`, `-noaccel`,
`-grabmouse`. `-disp` is handled by SDL via the `DISPLAY` env var; `-noaccel` has no
analog (SDL always uses its renderer); and the mouse is grabbed automatically during
gameplay (released in menus), so `-grabmouse` is unnecessary.

## Timing / misc / debug

| Parameter | Description |
|---|---|
| `-speed N` | Scale the game clock (10–1000; 100 = normal speed). |
| `-blockmap` | Enable the blockmap debug overlay. |
| `-debugfile` | Write engine debug output to `debug<N>.txt`. |
| `-cdrom` | DOS legacy: store config/saves under `c:/doomdata`. |

---

## Environment variables (SDL3 build)

| Variable | Description |
|---|---|
| `SMMU_SCALE=N` | Window magnification on top of the 640×400 framebuffer (1 → 640×400, 2 → 1280×800, …). |
| `SDL_VIDEODRIVER=dummy` | Run headless (no window) — useful for demo timing / smoke tests. |
| `SDL_AUDIODRIVER=dummy` | Use a null audio device (no sound output). |

## Examples

```
./smmu -iwad DOOM2.WAD                      # start DOOM II
./smmu -iwad doom1.wad -warp 1 -skill 4     # DOOM shareware, MAP01, Ultra-Violence
./smmu -iwad DOOM2.WAD -file MYMAP.wad      # load a PWAD
./smmu -iwad DOOM2.WAD -fast -respawn       # fast, respawning monsters
./smmu -iwad DOOM2.WAD -timedemo demo1      # benchmark a demo
SMMU_SCALE=2 ./smmu -iwad DOOM2.WAD         # 1280x800 window
```
