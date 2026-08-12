# SMACK!

A modernized **DOOM source port** for 64-bit Linux with an SDL3 backend.

SMACK! is a fork of **SMMU 3.21** ("Smack My Marine Up", 1999) by Simon Howard
("Fraggle") — itself an MBF-based port with Boom + MBF features, hub levels, player
skins, an in-game console, and FraggleScript. This fork brings that 1999 DOS/DJGPP
codebase up on modern hardware and toolchains, and adds sound, music, and a batch of
renderer and quality-of-life fixes.

## Highlights of this fork

- **Builds & runs on 64-bit Linux** (SDL3 backend) — fixed the era-specific 64-bit
  pointer and modern-compiler issues.
- **Sound** — SDL3 SFX mixer, plus authentic **OPL3 (Adlib) music** via Nuked-OPL3 +
  the IWAD's GENMIDI (no external soundfont).
- **Hi-res 640×400** by default; window scaling via `SMACK_SCALE` / `-2/-3/-4` / `-geom`.
- **HUD** — one screen-size control cycling status bar → fullscreen text overlay →
  GZDoom-style graphical HUD (full + 50%) → 50%-scaled status bar.
- **Textured automap** (aidoom-style).
- **Working key-bindings menu** (the original's was a dead link).
- **Renderer correctness** (ported from Woof): WiggleHack II (tall-wall shimmer),
  tall/DeePsea >254-row textures, long-wall wobble fix, overflow-safe BSP clipping,
  64-bit sprite clipping/projection, and more — all demo/netgame-safe.
- **Settings actually persist** (the original never wrote its config).

See [`docs/CHANGES.md`](docs/CHANGES.md) for the full summary and
[`docs/LEGACY_FIXES.md`](docs/LEGACY_FIXES.md) for the source-backed fix log.

## Building

### Linux

Requires `gcc`, `pkg-config`, and SDL3 development libraries.

```sh
make -f Makefile.sdl3          # release build -> obj/smack, copied into run/
make -f Makefile.sdl3 debug    # debug build
make -f Makefile.sdl3 clean
```

### Windows (x64) -- the easy way

From a normal command prompt, no environment setup needed:

```
build-mingw.bat            release build with mingw-w64
build-vs2019.bat           release build with Visual Studio 2019
build-vs2019.bat static    standalone single-file exe
```

Both find the toolchain themselves and deploy into `run\`. Add `debug`,
`clean` or `rebuild` as needed. The sections below describe the makefiles
these wrap.

### Windows (x64)

Requires a mingw-w64 gcc and an SDL3 SDK (the MSVC `SDL3-devel-VC` package works
— the build links straight against `SDL3.dll`). Produces a native `.exe` with no
Cygwin/MSYS runtime dependency.

```sh
make -f Makefile.mingw                                   # -> obj-win/smack.exe, copied into run/
make -f Makefile.mingw CC=x86_64-w64-mingw32-gcc SDL3_DIR=C:/SDL3
make -f Makefile.mingw clean
```

`run/` gets `smack.exe`, `SDL3.dll` and `smack.wad`; keep them together and drop
your IWAD in beside them. Every build in this repo deploys into `run/`.

### Windows (x64) with Visual Studio 2019

Requires VS2019 (Community or Build Tools) and the MSVC SDL3 SDK
(`SDL3-devel-VC`). From a VS2019 x64 developer prompt:

```
nmake /f Makefile.msvc                  # -> obj-msvc\smack.exe, copied into run\
nmake /f Makefile.msvc CFG=Debug
nmake /f Makefile.msvc clean
```

Or open `msvc\SMACK.sln` and press F7. Either way the runtime image lands in
`run\`. Set an `SDL3_DIR` environment variable (or pass `SDL3_DIR=...` to
nmake) if your SDK is not at `C:\Source\SDL3`.

By default the MSVC build links the dynamic CRT and SDL3.dll, so it needs the
Visual C++ redistributable and the DLL alongside the exe.

#### Standalone single-file exe

`STATIC=1` links both the C runtime and SDL3 into the executable, giving a
`smack.exe` that runs on a stock Windows install with no DLLs beside it and no
redistributable — it imports nothing but Windows' own system libraries. It needs
a static SDL3, which the SDL3-devel-VC SDK does not ship, so build one first
(once per SDL version, takes a few minutes):

```
msvc\build-sdl3-static.bat            # fetches + builds SDL3 -> C:\Source\SDL3-static
nmake /f Makefile.msvc STATIC=1       # -> run\smack.exe (standalone)
```

In the IDE, pick the **ReleaseStatic** configuration. Either way you get
`run\smack.exe` with no DLL beside it; copy it and `smack.wad` anywhere, add
your IWAD, and it is portable.

The mingw build remains dynamically linked against SDL3.dll (though it needs no
redistributable, since it uses the always-present `msvcrt.dll`).

## Running

You supply your own IWAD (`DOOM.WAD` / `DOOM2.WAD` / `doom1.wad`) — id's IWADs are
**not** included. Put it next to the binary (the `run/` directory is a ready-made
runtime image):

```sh
cd run
./smack -iwad DOOM2.WAD
./smack -iwad DOOM2.WAD -file MYMAP.wad     # load a PWAD (note: -file, not -wad)
./smack -iwad doom1.wad -warp 1 -skill 4
```

See [`docs/PARAMETERS.md`](docs/PARAMETERS.md) for every command-line parameter.
(The binary resolves its config/PWAD names from `argv[0]`, so it ships as `smack`
alongside `smack.wad`/`smack.cfg`.)

## Credits & license

- **SMMU** and its FraggleScript engine: Simon Howard ("Fraggle").
- Based on **MBF** (Lee Killough) and **Boom** (TeamTNT), themselves based on id
  Software's DOOM.
- Renderer fixes ported from **Woof!** (and its lineage: prboom / Crispy Doom /
  Nuked-OPL3), which are GPL — see those projects for their terms.

The original DOOM source is distributed under id Software's DOOM Source Code License.
This is a personal modernization fork; keep the original license headers intact.
