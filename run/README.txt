SMACK!
======

This is the runtime directory. Every build in the repository -- Linux, Windows
(mingw-w64) and Windows (Visual Studio) -- drops its output here, so whatever
you built last is what you run.

The binary works out where its data lives from its own path (argv[0]), so
smack/smack.exe, smack.wad and SDL3.dll must stay together in this folder.


Contents
--------

  smack           Linux binary (built by `make -f Makefile.sdl3`)
  smack.exe       Windows binary (Makefile.mingw, Makefile.msvc or the VS
                  solution). The Linux and Windows binaries coexist here;
                  two different *Windows* builds overwrite each other.
  SDL3.dll        Windows only, and only for the dynamically-linked builds.
                  The `nmake /f Makefile.msvc STATIC=1` build has SDL3 inside
                  the .exe and needs no DLL at all.
  smack.bat       Windows convenience launcher (see below).
  ID0\            the data directory -- everything else lives in here:

      smack.wad       SMACK's own PWAD. Required. On Linux this is a symlink
                      to ../../smack.wad; on Windows it is a copy.
      <your IWAD>     DOOM2.WAD, doom1.wad, ... -- drop it in here.
      <your PWADs>    -file looks here too, so `-file mymap.wad` works
                      without a path.
      smack.cfg       your settings. Written on exit, created on first run.
      savegames\      your saves.

You supply your own IWAD -- none is included.


Quick start
-----------

Linux:

  cd run
  ./smack -iwad DOOM2.WAD             # DOOM II
  ./smack -iwad doom1.wad -warp 1     # shareware, straight to E1M1
  ./smack -iwad DOOM2.WAD -skill 4    # Ultra-Violence

Windows:

  cd run
  smack.exe -iwad DOOM2.WAD
  smack.bat -warp 1 -skill 4          # same thing; smack.bat cd's here first

Drop an IWAD into this folder and plain `./smack` (or double-clicking
smack.bat) is enough -- see the search order below.


Where SMACK looks for the IWAD
------------------------------

In order, stopping at the first hit (FindIWADFile() in d_main.c):

  1. -iwad, if you passed it.
       -iwad <file>  uses that file directly.
       -iwad <dir>   searches that directory for the standard names below.
       -iwad <name>  a bare name with no path is remembered as a custom name
                     and looked for in the steps below instead of the
                     standard ones.
       A missing ".wad" extension is added automatically.
  2. The ID0 data directory. This is the normal place to keep your IWAD.
  3. The current working directory, then the directory containing the binary.
  4. The DOOMWADDIR environment variable, then HOME. Either may name a file
     directly or a directory to search.
  5. Steam. SMACK finds Steam itself (from the registry on Windows, from the
     usual locations on Linux and macOS, including Flatpak) and reads
     steamapps\libraryfolders.vdf so games on other drives are covered. It
     then looks in the DOOM installs: the classic DOSBox copies under
     base\ (including the Final Doom and DOOM II subfolders), the BFG
     Edition, and the 2024 "DOOM + DOOM II" re-release under rerelease\.

     Steam comes last, so an IWAD you put in ID0 always wins.

     Caveat: the 2024 re-release IWADs are searched last of all, because they
     carry ID24 extensions this 1999-derived renderer does not understand --
     loading rerelease\doom2.wad crashes during R_Init. Steam now ships both
     packagings together, so the classic copy is picked automatically and it
     works. If the re-release is all you have, use the classic IWAD from
     another source.

Within steps 2-5 the standard IWAD names are tried in this order:

    doom2f.wad  doom2.wad  plutonia.wad  tnt.wad  doom.wad  doom1.wad

so if several IWADs sit side by side, DOOM II wins over Plutonia, TNT, and
DOOM 1. Pass -iwad explicitly when you want a specific one.

Note the names in that list are lowercase. Windows does not care, but on Linux
and macOS the filesystem is case-sensitive: a file called DOOM2.WAD in ID0 will
NOT be found by the automatic search. Either rename it to doom2.wad or name it
with -iwad. (The Steam search does try both spellings, because Steam's classic
packaging ships DOOM.WAD in capitals.)

If nothing is found, SMACK exits with "IWAD not found".


Files SMACK writes
------------------

  ID0\smack.cfg     every console variable: screen size, HUD style, key
                    bindings, automap options, gamma, volumes. Written on a
                    clean exit, so quit properly rather than killing the
                    process.
  ID0\savegames\    save slots. -save DIR puts them somewhere else.
  tranmap.dat       a cached translucency table, regenerated if deleted.


Display
-------

Rendering is always hi-res 640x400. The window can be magnified on top of
that; in order of precedence:

  -geom 1280x800     explicit window size
  -2 / -3 / -4       2x, 3x or 4x
  SMACK_SCALE=N      environment variable
  (default)          1x, a 640x400 window

The window size is not saved in smack.cfg -- it is decided at each launch.

Screen size / HUD is the "screen size" slider in the menu (cvar screensize,
0-11): 0-7 give a windowed view with the status bar, 8 is fullscreen with the
classic text overlay, 9 and 10 are a graphical HUD at full and half size, and
11 is the vanilla status bar at half size.


Sound
-----

Sound effects and music both work. Music is authentic OPL3 (Adlib) synthesis
using the IWAD's own GENMIDI patches -- no soundfont or external synth needed.
Use -nosound / -nomusic to skip either, or SDL_AUDIODRIVER=dummy for a silent
headless run.


Controls
--------

Defaults are the classic Doom ones: Ctrl fires, Space uses, Alt strafes,
Shift runs, Tab is the automap, and the backtick key (`) opens the console.
Everything is rebindable in Options -> key bindings, and bindings persist in
smack.cfg.


More
----

  docs/PARAMETERS.md    every command-line option
  docs/CHANGES.md       what this fork changed
  ../README.md          build instructions for all three toolchains
