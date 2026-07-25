SMMU — Smack My Marine Up
==========================

Linux SDL3 build. Files in this directory are a self-contained run-time
image: just `cd run && ./smmu ...` and you're playing.

Contents
--------

  smmu            the 7.4 MB ELF binary, SDL3 statically linked. Only
                  libc/libm are dynamic (standard system libs).
  smmu.wad        SMMU's own PWAD (symlink to ../smmu.wad -- it must
                  live in the same directory as the binary because
                  D_DoomExeDir() builds the path from argv[0]).
  DOOM2.WAD       DOOM II retail IWAD (Episode 1-3 + Secret).
  DOOM.WAD        DOOM 1 retail IWAD (4 episodes).
  doom1.wad       DOOM 1 shareware IWAD (Episode 1 only).
  hubtest.wad     test WAD for hub-level transitions.
  smmutest.wad    test WAD for engine features.

Quick start
-----------

  cd run
  ./smmu -iwad DOOM2.WAD                # start DOOM II
  ./smmu -iwad doom1.wad                # DOOM 1 shareware
  ./smmu -iwad DOOM2.WAD -warp 1        # jump straight to MAP01
  ./smmu -iwad DOOM2.WAD -skill 4       # Ultra-Violence from the start

Common flags
------------

  -iwad FILE      which IWAD to use (REQUIRED on first run; SMMU
                  auto-detects mode from the IWAD header).
  -warp N         jump to map N on start.
  -skill N        1=ITYTD  2=HNTR  3=HMP  4=UV  5=NM.
  -fast           monsters move and attack faster.
  -respawn        monsters respawn after death.
  -noload         skip the "wadfile_1/2" preinclude from smmu.cfg
                  (handy when the defaults point at a stale path).
  -nosound        skip sound system init.
  -nomusic        skip music system init.
  -nodraw         skip video init entirely (server-style smoke test).
  -timedemo FILE  play a demo/lmp as fast as possible; prints stats.
  -playdemo FILE  play a demo at normal speed.

In-game controls (defaults)
---------------------------

  arrows / WASD   move / turn
  Ctrl            fire
  Space           use / open doors
  , / .           strafe left/right
  1-7             select weapon
  F1              help screen
  F2              save game
  F3              load game
  F4              sound volume
  F5              gamma correction
  F6              quick save
  F7              quick load
  F8              toggle message scroll
  F9              quick load
  F10             quit
  F11             gamma down
  F12             gamma up
  Tab             automap
  `               console (FraggleScript enabled)
  Pause           pause game

Files written to $HOME
----------------------

SMMU writes `~/.smmu/smmu.cfg` for user settings and `~/.smmu/savegames/`
for save slots on first run.

System requirements
-------------------

  Linux x86_64 (3.2+)
  X11 or Wayland (SDL3 picks automatically), or run with SDL_VIDEODRIVER=dummy
  ~50 MB disk, ~80 MB RAM
  A pulseaudio / pipewire / oss audio stack if you want sound.

Known issues
------------

  - Sound: linux/i_sound.c opens an SDL3 audio stream but currently
    plays silence. Wire up SDL3_mixer if you want SFX/music.
  - The "smmu.wad" symlink must resolve relative to the binary;
    running from a different cwd without the symlink triggers
    D_DoomExeName's hard-coded path lookup.
  - Audio init can deadlock on some headless setups -- SDL_AUDIODRIVER=dummy
    bypasses it.
  - Render scale defaults to 1x (320x200 native). On systems with
    tight memory you can stay there; for bigger windows set
    SMMU_SCALE=2 (640x400) or SMMU_SCALE=4 (1280x800) before launch.
  - SMMU itself needs ~100 MB RSS once it has loaded a WAD. On a
    6 GB system with several browsers / AI tools / dev tooling
    open, the Linux OOM-killer can grab SMMU's first 5-second
    "load everything" pass. If you see "Killed" with no other
    output, free ~500 MB of RAM (close a browser tab) and try again.
