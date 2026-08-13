# Running SMACK!

Everything the player needs: where files go, how the IWAD is found, and what
the display/sound/input options do. For build instructions see the top-level
[`README.md`](../README.md); for every command-line switch see
[`PARAMETERS.md`](PARAMETERS.md).

## The `run/` directory

Every build in the repository — Linux, Windows/mingw and Windows/MSVC — deploys
into `run/`, so whatever you built last is what you run. The binary works out
where its data lives from its own path (`argv[0]`), so the executable and its
libraries must stay together:

| file | notes |
|---|---|
| `smack` | Linux binary (`make -f Makefile.sdl3`) |
| `smack.exe` | Windows binary. The Linux and Windows binaries coexist here; two different *Windows* builds overwrite each other |
| `SDL3.dll` | Windows only, and only for the dynamically-linked builds. The standalone build (`STATIC=1`) has SDL3 inside the `.exe` and needs no DLL |
| `smack.bat` | Windows launcher — `cd`s to its own directory and forwards its arguments |
| `ID0/` | the data directory — everything else lives in here |

### `ID0/` — the data directory

| file | notes |
|---|---|
| `smack.wad` | SMACK's own PWAD. Required. A symlink to `../../smack.wad` on Linux, a copy on Windows |
| *your IWAD* | `DOOM2.WAD`, `doom1.wad`, … — drop it in here |
| *your PWADs* | `-file` looks here too, so `-file mymap.wad` works without a path |
| `smack.cfg` | your settings, written when you leave a menu and on exit |
| `savegames/` | save slots (`-save DIR` puts them elsewhere) |

You supply your own IWAD — none is included.

## Quick start

Linux:

```sh
cd run
./smack                              # uses whatever IWAD it can find
./smack -iwad DOOM2.WAD              # DOOM II
./smack -iwad doom1.wad -warp 1      # shareware, straight to E1M1
./smack -iwad DOOM2.WAD -skill 4     # Ultra-Violence
```

Windows:

```
cd run
smack.exe -iwad DOOM2.WAD
smack.bat -warp 1 -skill 4           # same thing; smack.bat cd's here first
```

Drop an IWAD into `ID0/` and plain `./smack` (or double-clicking `smack.bat`)
is enough.

## Where SMACK looks for the IWAD

In order, stopping at the first hit (`FindIWADFile()` in `d_main.c`):

1. **`-iwad`**, if you passed it.
   - `-iwad <file>` uses that file directly
   - `-iwad <dir>` searches that directory for the standard names below
   - `-iwad <name>` — a bare name with no path is remembered as a custom name and
     looked for in the steps below instead of the standard ones

   A missing `.wad` extension is added automatically.
2. **The `ID0` data directory.** The normal place to keep your IWAD.
3. **The current directory**, then **the directory containing the binary**.
4. **`$DOOMWADDIR`**, then **`$HOME`**. Either may name a file directly or a
   directory to search.
5. **Steam.** SMACK finds Steam itself — from the registry on Windows, from the
   usual locations on Linux and macOS including Flatpak — and reads
   `steamapps/libraryfolders.vdf`, so games installed on another drive are
   covered. It then looks in the DOOM installs: the classic DOSBox copies under
   `base/` (including the Final Doom and DOOM II subfolders), the BFG Edition,
   and the 2024 *DOOM + DOOM II* re-release under `rerelease/`.

Steam comes last, so an IWAD you put in `ID0` always wins.

Within a directory the standard IWAD names are tried in this order:

```
doom2f.wad  doom2.wad  plutonia.wad  tnt.wad  doom.wad  doom1.wad
```

so with several IWADs side by side, DOOM II wins over Plutonia, TNT and DOOM 1.
Pass `-iwad` when you want a specific one.

> **Case sensitivity.** Those names are lowercase and the lookup is a plain
> `stat()`. Windows does not care, but on Linux and macOS a file called
> `DOOM2.WAD` in `ID0` will **not** be found by the automatic search — rename it
> lowercase or name it with `-iwad`. The Steam search is the exception: it tries
> each name as given, lowercased and uppercased, because Steam's classic
> packaging ships `DOOM.WAD` in capitals.

> **The 2024 re-release.** Its IWADs carry ID24 extensions this 1999-derived
> renderer does not understand — loading `rerelease/doom2.wad` crashes during
> `R_Init`. They are searched *last* for that reason, and since Steam now ships
> both packagings together under "Ultimate Doom" the classic copy is picked
> automatically. If the re-release is all you have, use a classic IWAD from
> another source.

If nothing is found, SMACK exits with "IWAD not found".

## Display

Rendering is always hi-res 640×400. The window is magnified on top of that; in
order of precedence:

| | |
|---|---|
| `-geom 1280x800` | explicit window size |
| `-2` / `-3` / `-4` | 2×, 3× or 4× |
| `SMACK_SCALE=N` | environment variable |
| *(default)* | 1×, a 640×400 window |

The window size is not saved — it is decided at each launch.

**Screen size / HUD** is the "screen size" slider in the menu (cvar
`screensize`, 0–11):

| value | result |
|---|---|
| 0–7 | windowed 3D view with the status bar |
| 8 | fullscreen with the classic text overlay |
| 9 | fullscreen with a graphical HUD |
| 10 | the same HUD at half size |
| 11 | the vanilla status bar at half size, centred |

**Dithered lighting** (Options → video → rendering → "dither light levels",
cvar `r_dither`, default on) smooths the hard seams between Doom's 32 light
bands, which are most visible as banding in shadows and across floors.

## Sound

Sound effects and music both work. Music is authentic **OPL3 (Adlib)**
synthesis using the IWAD's own `GENMIDI` patches — no soundfont or external
synth needed. `-nosound` / `-nomusic` skip either; `SDL_AUDIODRIVER=dummy`
gives a silent headless run.

## Controls

Defaults are the classic Doom ones: **Ctrl** fires, **Space** uses, **Alt**
strafes, **Shift** runs, **Tab** is the automap, and the backtick key
(<kbd>`</kbd>) opens the console. Everything is rebindable in
**Options → key bindings**, and bindings persist in `ID0/smack.cfg`. In the
"press a key to bind" prompt, **Delete** clears the binding (it then shows `---`)
and **Escape** cancels without changing it.

**Gamepad**: enable it with Options → mouse options → "enable joystick"
(cvar `use_joystick`). The first pad found is used, hotplug included; the left
stick and d-pad move, and the face buttons map to the `joyb_*` bindings —
by default A/right-trigger fire, X strafes, left-shoulder runs, and
B/right-shoulder is use. `-nojoy` skips gamepad init entirely.

**Weapons**: Options → weapons → "switch on pickup" (cvar
`weapon_autoswitch`, default on) controls whether picking a weapon up switches
to it. The setting is ignored in netgames and demos, which always use vanilla
behaviour.

## Where settings are written

| file | notes |
|---|---|
| `ID0/smack.cfg` | every console variable: screen size, HUD style, key bindings, automap options, gamma, volumes. Written when you leave a menu, and again on exit |
| `ID0/savegames/` | save slots; `-save DIR` overrides |
| `tranmap.dat` | cached translucency table, regenerated if deleted |
