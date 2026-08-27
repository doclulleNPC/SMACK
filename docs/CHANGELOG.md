# Changelog

Every change since the unmodified SMMU 3.21 (1999) source, newest first.

This is the chronological record. For a grouped, thematic overview of what the
fork adds see [`CHANGES.md`](CHANGES.md); for the source-backed detail on
individual portability fixes (symptom → root cause → fix → files) see
[`LEGACY_FIXES.md`](LEGACY_FIXES.md).

The starting point was the 1999 DOS/DJGPP tree plus a partial, non-building
SDL3 Linux backend under `linux/`.

---

## Unreleased

### Widescreen: centre the status bar, game visible either side

At screensize 7 (the default, `setblocks == 10`) the 3D view now fills the
screen **entirely** -- full width *and* full height -- with the status bar drawn
over the bottom of it as a centred overlay, so the level is visible to the left
and right of the bar instead of the view stopping at its edges. This is what
aidoom does (`files/r_main.c:717`).

- `R_ExecuteSetViewSize`: `setblocks == 10` takes `scaledviewheight =
  SCREENHEIGHT` as well as the full width.
- `st_stuff.c`: every horizontal status-bar coordinate (`ST_X`, `ST_FX`,
  `ST_AMMOX`, `ST_HEALTHX`, `ST_ARMSX`, `ST_FRAGSX`, `ST_ARMORX`, the key and
  ammo columns, `ST_FACESX`, …) now carries `+ deltawidth`, aidoom's
  `WIDESCREENDELTA` approach (`files/st_stuff.c:87`). They stay macros rather
  than becoming constants so they follow a live aspect change; the widget
  structs capture them in `ST_createWidgets`, which `I_ApplyAspect` re-runs via
  `ST_Start`. At the classic aspect `deltawidth` is 0, so every value is its
  vanilla one and nothing moves.
- `ST_Drawer` forces a **full** bar redraw whenever the bar is shown over a
  full-height view. The incremental `ST_diffDraw` path assumes the untouched
  parts of the bar survive from the previous frame, which stops being true once
  the 3D view repaints that area every frame -- only the widgets that happened
  to change would have been drawn.
- `D_Display` asks the screen-size tier (`screenSize >= 8`) whether the bar is
  hidden, rather than inferring it from `scaledviewheight == 200`. Those meant
  the same thing until screensize 7 also became full-height.

Demo playback clean at screensizes 4, 7, 8 and 11 on a 1920x1080 window.

### Widescreen: fix the view being stuck 4:3 against one edge

**Bug report:** at a widescreen ratio the window just got bigger while the game
stayed 4:3 on the left, with dead space to its right. Two mistakes of mine:

1. `R_ExecuteSetViewSize` computed `scaledviewwidth = setblocks*32`, hardwired to
   the 320 grid, so every screen size below fullscreen kept a classic-width 3D
   view no matter how wide the screen got. **The default screensize (7,
   `setblocks == 10`) is one of those**, which is why it looked broken out of
   the box.
2. The previous entry deliberately referenced `BASE_WIDTH` in `R_InitBuffer` to
   "preserve windowed positioning". That is what pinned the view to the left
   edge: the vanilla expression `(SCREENWIDTH-width)>>!hires` *centres* it, and
   overriding it was simply wrong.

Fixed by following Woof (`src/r_main.c:534`), Crispy
(`src/doom/r_main.c:810`) and aidoom (`files/r_main.c:705`), which all agree:

- `setblocks == 10`, the largest *windowed* size, now takes the **full** widened
  width, reducing only the height to leave room for the status bar.
- Smaller sizes scale their width to the screen's shape
  (`scaledviewheight * SCREENWIDTH / (BASE_HEIGHT - ST_HEIGHT)`) instead of
  staying 320-based, rounded **up to a multiple of 8** — the bezel patch width —
  so `R_FillBackScreen`'s border tiles evenly on both sides. That is Crispy's
  `widescreen_edge_aligner` trick, and it removes the reason the previous entry
  gave for skipping the border at widened sizes; both border-skip conditions are
  back to the honest `scaledviewwidth == SCREENWIDTH`.
- `R_InitBuffer` centres the view again (vanilla expression restored).
- New `scaledviewwidth_nonwide` / `centerxfrac_nonwide` hold the classic
  320-based reference for each tier, mirroring Woof. The widescreen FOV branch
  now triggers on `centerxfrac != centerxfrac_nonwide` (Woof `src/r_main.c:332`)
  rather than a fullscreen-only test, so the aspect-scaled windowed sizes get
  the widened FOV too.
- **`projection` now uses `centerxfrac_nonwide`**, which the first widescreen
  commit got wrong. Taking the *wide* half-width scales the whole projection
  (Vert-, zooming in vertically); the non-wide reference extends the view
  horizontally instead (Hor+), which is what widescreen should do and what Woof
  (`src/r_main.c:338`) and aidoom ("Hor+: vertical FOV from the 4:3 ref") both
  do. Identical to the old expression whenever the screen is not widened.

Verified at a 1920x1080 window: the view width is now 356 (= full `SCREENWIDTH`)
at the default screensize 7 where it was previously 320 and left-aligned, 356 at
fullscreen, and 104 / 240 (8-aligned, centred) at the small windowed sizes; demo
playback clean at windowed, largest-windowed and fullscreen tiers.

### Widescreen: aspect ratio option

The widescreen work in the entry below shipped with **no user-facing option**
and re-derived its width only at startup. Both gaps are now closed.

- New cvar **`v_aspect`** (`linux/i_video.c`), exposed as **Options -> video ->
  "aspect ratio"** and persisted in `smack.cfg`: `classic 320x200`,
  `auto (match window)` (the default), `16:9`, `21:9`, `32:9`. Modelled on
  Woof's `aspect_ratio_mode_t` (`src/i_video.c:100`).
- Deliberately **no 16:10 entry**, unlike Woof: SMACK renders a 320x200 grid
  and stretches it to the window without the 1.2x vertical aspect correction
  other ports apply, so its native grid *is* 16:10 and such a mode would be a
  silent duplicate of `classic`. (Woof has both because its `RATIO_ORIG` is
  aspect-corrected 4:3.) By the same token these ratios mean "fill a window of
  this shape", not "this shape with square pixels" -- the missing aspect
  correction is long-standing SMACK behaviour, not something widescreen
  introduced, and is still unaddressed.
- `MAX_SCREENWIDTH` raised 1024 -> 1536. At 1024 the 32:9 mode was silently
  clamped to 512 base columns (~2.56:1) instead of its true 711; 1536 covers
  `BASE_HEIGHT*32/9 = 711` columns = 1422 real pixels at hires.
- The derivation is refactored out of `I_CreateWindowAndRenderer` into
  `I_DeriveWidescreen` (computes `SCREENWIDTH`/`deltawidth`, returns whether
  it changed) and `I_CreateFramebuffer` (resizes `screens[]`, the ARGB surface
  and the texture). `I_ApplyAspect` combines them and sets `setsizeneeded`, so
  the renderer rebuilds its view-size tables on the next frame. Unlike
  `I_ResetScreen` this keeps the window alive, so it is cheap enough to run
  from a resize event.
- **`I_ApplyAspect` is now called from the three paths that previously did
  nothing**: the `v_aspect` cvar handler (so the menu applies live), the
  `SDL_EVENT_WINDOW_RESIZED` handler (so `auto` follows a dragged window), and
  `I_SetFullscreen` (which calls `SDL_SetWindowFullscreen` directly and never
  routed through `I_ResetScreen`). The changelog entry below originally
  claimed the fullscreen toggle already re-derived everything -- that was
  wrong, and is corrected there.

Verified: all five modes produce the expected distinct widths at a 1920x1080
window (320 / 356 / 356 / 467 / 711), each through a full demo playback with no
crash; and -- the risky path, since it reallocates `screens[]` mid-game while
`ylookup[]` points into it -- a **live window resize** on a real window drove
320 -> 493 -> 320 (widen, then shrink back past the classic clamp) with the
game still rendering at full rate throughout and no crash.

Still not verified visually: whether the widened view actually *looks* right.

### Widescreen

The render framebuffer now widens to match the real window/display aspect
ratio instead of always being a fixed 640x400 (classic 8:5) image stretched
to whatever window size the player picked. `deltawidth` (new global,
`doomdef.c`/`.h`) is `(SCREENWIDTH - BASE_WIDTH) / 2`, 0 at the classic aspect.

- `linux/i_video.c`'s `I_CreateWindowAndRenderer` derives `SCREENWIDTH` from
  `SDL_GetRenderOutputSize` (the real output pixels, correct for a fullscreen
  window even when that differs from what was requested) once the window
  exists, clamped so it never goes narrower than the classic aspect ratio
  (widescreen only ever shows more at the sides) and never exceeds
  `MAX_SCREENWIDTH`, which was raised from 640 to 1024 to give the wider
  framebuffer room. `V_Init()` (now idempotent -- frees its previous buffers
  before reallocating) runs again here to resize `screens[]`, since the
  startup call in `d_main.c` runs before the real aspect ratio is known and
  sizes it at the classic width.
  (**Correction**, see the "Widescreen: aspect ratio option" entry above: as
  originally committed this only ever ran at *startup*. This entry claimed a
  fullscreen toggle re-derived the width; it did not -- `I_SetFullscreen`
  never went through `I_ResetScreen`, and the window-resize handler didn't
  re-derive either. Both are fixed in the later entry.)
- `R_InitTextureMapping` (`r_main.c`) widens the field of view to match, the
  Woof way (`src/r_main.c:318-353`): when the view genuinely fills a widened
  screen, `fov' = 2*atan(tan(fov/2) * width_ratio)`, where `width_ratio` is
  how much wider `centerxfrac` is than its classic reference. Without this,
  a wider screen would just render the same 90-degree view at higher
  resolution rather than showing more of the world -- the whole point.
- Everything downstream (BSP/segs/plane/things column drawers, `screens[]`
  row-stride math in `v_video.c`, the crosshair/message centering in
  `hu_stuff.c`, the graphical HUD in `hu_over.c`, the automap frame, the
  aidoom-style 50% status bar, the intermission/finale drawing in
  `wi_stuff.c`/`f_finale.c`) was already `SCREENWIDTH`-relative from earlier
  groundwork and needed no changes -- it turns out most of the engine was
  already prepared for this. The one straggler: `hu_leveltime`'s widget
  position, initialised at a `BASE_WIDTH`-relative constant (a static
  initializer can't reference the runtime `deltawidth`), now gets shifted by
  `deltawidth` once in `HU_WidgetsInit`.
- Windowed (sub-fullscreen) screen sizes deliberately keep their exact
  pre-widescreen positioning, pixel for pixel -- this pass only widens the
  fullscreen tier. `R_InitBuffer`'s view-margin formula and the
  `R_FillBackScreen`/`R_DrawViewBorder` border-skip conditions (`r_draw.c`)
  now reference `BASE_WIDTH` rather than the live `SCREENWIDTH` for the
  windowed case, so a widened screen doesn't change what a windowed view
  looks like. The corner-patch drawing in those two functions was never
  written to handle a zero margin on one side and a real one on the other
  (which the largest windowed size -- `scaledviewwidth == BASE_WIDTH` -- would
  otherwise hit once the live screen is wider), so it keeps the same
  historical skip condition rather than risk it. One accepted consequence: at
  that specific windowed size, the extra widescreen margin is not filled with
  anything, unlike the genuine fullscreen tier.
- Also fixed in passing, found while chasing a crash this work exposed:
  `R_DrawFuzzColumn`'s `RANGECHECK` bounds guard (`r_draw.c`) compared the
  pixel-space `dc_x`/`dc_yh` against the base-space `SCREENWIDTH`/
  `SCREENHEIGHT` instead of `MAX_SCREENWIDTH`/`MAX_SCREENHEIGHT`, unlike every
  sibling column drawer -- a false-positive `I_Error` for any fuzz (partial
  invisibility) column in the right half of the screen, in a `RANGECHECK`
  build. Harmless in a normal release build (the code compiles out entirely),
  which is presumably why it went unnoticed.
- The duplicate, dead `#define BASE_WIDTH 800` in `doomdef.h` (an old
  DOS-era leftover, shadowed by the real `BASE_WIDTH 320` defined later in
  the same file) was removed while working in this area.

Verified headlessly (`SDL_VIDEODRIVER=dummy`) across 640x400 (classic), 16:9,
21:9 and a narrower-than-classic 800x600, at three screen-size tiers (small
windowed, largest windowed, fullscreen), each run through a full demo
playback with no crash. Not verified visually -- nothing here has been seen
rendered on an actual display.

Outstanding: only the fullscreen 3D view widens. Automap text overlays, the
menu background, and the classic text-overlay HUD (screensize 8) were not
individually audited for centering and may not be perfectly positioned in
the new widescreen margin, though nothing found so far draws outside it.

### MBF21: thing flags

`mobjflag2_t` (`p_mobj.h`, from Woof `src/p_mobj.h:207`) with all 19 mbf21 thing
flags, stored in `mobjinfo_t.flags2` and copied to `mobj_t.flags2` at spawn. Set
per thing type by the DEHACKED `MBF21 Bits` field, which takes the same mnemonic
syntax as `Bits` (`LOGRAV|RIP|...`), and at runtime by `A_AddFlags`,
`A_RemoveFlags` and `A_JumpIfFlagsSet`. `Rip sound` was added alongside
(`mobjinfo_t.ripsound`).

Behaviour, all of it keeping the existing hardcoded type checks and adding the
flag beside them, so vanilla things behave exactly as before:

- `MF2_LOGRAV` — eighth gravity in `P_ZMovement`.
- `MF2_SHORTMRANGE`, `MF2_LONGMELEE`, `MF2_RANGEHALF`, `MF2_HIGHERMPROB` —
  `P_CheckMissileRange`, previously `MT_VILE` / `MT_UNDEAD` / `MT_CYBORG` etc.
- `MF2_DMGIGNORED`, `MF2_NOTHRESHOLD` — infighting in `P_DamageMobj`.
- `MF2_NORADIUSDMG`, `MF2_BOSS`, `MF2_FORCERADIUSDMG` — blast immunity in
  `PIT_RadiusAttack`.
- `MF2_BOSS`, `MF2_FULLVOLSOUNDS` — full-volume see and death sounds.
- `MF2_RIP` — ripper projectiles pass through what they hit (`PIT_CheckThing`).
- `MF2_MAP07BOSS1/2`, `MF2_E1M8BOSS` … `MF2_E4M8BOSS` — `A_BossDeath`, both the
  type gate and the 666/667 tag dispatch.

**Savegames from earlier builds will not load.** `mobj_t` is written verbatim, so
adding `flags2` changed the format. Rather than let an old save be misread,
`SMACK_SAVE_REV` now rides along in the existing 16-byte version field, so
`G_DoLoadGame` rejects them with the usual "Different Savegame Version" prompt.
Bump it whenever a saved struct changes.

This completes MBF21 apart from DSDHacked (unlimited state/thing/sprite arrays).

### MBF21: weapon codepointers

Ported from Woof (`src/p_pspr.c:1240-1520`): `A_WeaponProjectile`,
`A_WeaponBulletAttack`, `A_WeaponMeleeAttack`, `A_WeaponSound`, `A_WeaponAlert`,
`A_WeaponJump`, `A_ConsumeAmmo`, `A_CheckAmmo`, `A_RefireTo`, `A_GunFlashTo`.

- `weaponinfo_t.ammopershot` + DEHACKED `Ammo per shot` in the Weapon block.
  `d_items.c` initialises the table positionally and stops short of the field, so
  it is 0 until a patch sets it — which is what `A_ConsumeAmmo`/`A_CheckAmmo`
  treat as "fall back to args".
- `P_SetPspritePtr` addresses a psprite by pointer instead of layer index.
- Woof’s `A_Recoil` is its view-pitch recoil, which SMACK does not have. The
  parameterized attacks instead call `P_WeaponRecoil`, the recoil half of
  `A_FireSomething` factored out, so they thrust the player exactly as SMACK’s
  built-in weapons do (still gated on the `weapon_recoil` cvar).
- `S_StartSoundOrigin`/`S_StartSoundEx` become plain `S_StartSound`; a NULL origin
  is how SMACK plays a sound at full volume, which is what `A_WeaponSound`’s
  second arg selects.
- Woof’s `SavePlayerAngle`/`AddToTicAngle` view bookkeeping is dropped —
  `A_WeaponMeleeAttack` assigns the turn-to-target angle directly, as SMACK’s own
  `A_Punch` does.

### MBF21: monster codepointers

The parameterized monster pointers, ported from Woof (`src/p_enemy.c:2865-3271`):
`A_SpawnObject`, `A_MonsterProjectile`, `A_MonsterBulletAttack`,
`A_MonsterMeleeAttack`, `A_RadiusDamage`, `A_NoiseAlert`, `A_HealChase`,
`A_SeekTracer`, `A_FindTracer`, `A_ClearTracer`, `A_JumpIfHealthBelow`,
`A_JumpIfTargetInSight`, `A_JumpIfTargetCloser`, `A_JumpIfTracerInSight` and
`A_JumpIfTracerCloser`. All are reachable from a DEHACKED `[CODEPTR]` block.

Supporting work:

- `state_t` gained `args[MAXSTATEARGS]` (`info.h`), set from a Frame block via
  `Args1`..`Args8`. `info.c` initialises states positionally with five fields,
  so the new members zero-fill.
- `tables.h`: `FixedToAngle`, `AngleToSlope`, `DegToSlope` (Woof `src/tables.h:81`,
  originally Eternity).
- `m_random`: `pr_mbf21` appended to the RNG classes (appended, so existing class
  indices and therefore demo compatibility are untouched), plus
  `P_RandomHitscanAngle` / `P_RandomHitscanSlope`.
- `P_CheckFov` (`p_sight.c`), `P_RoughTargetSearch` (`p_maputl.c`, Hexen by way of
  Woof), `P_SeekerMissile` / `P_FaceMobj` (`p_mobj.c`).
- `A_VileChase` refactored into `P_HealCorpse(actor, radius, healstate, healsound)`
  so `A_HealChase` can share it; `PIT_VileCheck` now reads `viletryradius` instead
  of hardcoding the archvile’s. `A_VileChase` passes the vile’s own radius, so its
  behaviour is unchanged.
- `P_RadiusAttack` gained a `distance` parameter for `A_RadiusDamage`. When damage
  and distance are equal the falloff reduces to the vanilla formula, so `A_Explode`
  and the other existing callers are bit-for-bit unchanged.
- `mobjinfo_t.meleerange` + DEHACKED `Melee range` (`DEH_MOBJINFOMAX` 23 → 24).
  The `info.c` table stops short of the new field, so it is 0 until a patch sets
  it, and `A_MonsterMeleeAttack` reads 0 as "use `MELEERANGE`".

Deviations from Woof: SMACK has no complevel system, so the `mbf21` guard on each
pointer is dropped — a state only reaches one of these if a patch pointed it there.
SMACK also has no damage-type plumbing, so `A_MonsterMeleeAttack` calls plain
`P_DamageMobj` rather than `P_DamageMobjBy(..., MOD_Melee)`.

Still outstanding for MBF21: the weapon codepointers, the thing flags (`flags2`)
with `A_AddFlags` / `A_RemoveFlags` / `A_JumpIfFlagsSet`, and DSDHacked.

### MBF21: instant-death sectors

- `DEATH_MASK` (bit 12) and `KILL_MONSTERS_MASK` (bit 13) added to `p_spec.h`,
  following Woof (`src/p_spec.h:85`).
- `P_PlayerInSpecialSector` (`p_spec.c`) handles `DEATH_MASK` ahead of the ordinary
  nukage switch, with the damage bits selecting the variant: 0 kills unless the
  player is invulnerable or wearing a radiation suit, 1 kills unconditionally,
  2 and 3 kill every player and end the level (normal / secret exit).
  Being checked first, these sectors also ignore the nukage-disabling cheat.
- `P_MobjThinker` (`p_mobj.c`) handles `KILL_MONSTERS_MASK`: shootable,
  non-floating, grounded non-players are killed (Woof `src/p_mobj.c:807`).

### DSDHacked: unlimited state and thing arrays

`d_dsdh.c`/`.h` (new), ported from dsda-doom/Woof's DSDHacked feature
(originally by Xaser Acheron/Kraflab). `states[NUMSTATES]` and
`mobjinfo[NUMMOBJTYPES]` are no longer fixed-size: `info.c`'s compile-time
tables are renamed `original_states`/`original_mobjinfo`, and `states`/
`mobjinfo` become malloc'd copies (`DSDH_Init`, called at the top of
`D_DoomMain`) that grow on demand as `DSDH_StateTranslate`/`DSDH_ThingTranslate`
hand out fresh slots for DEHACKED indices beyond the original range. `NUMSTATES`/
`NUMMOBJTYPES` remain as compile-time constants — they still name the fixed
`statenum_t`/`mobjtype_t` enumerators — so every runtime loop bound that used to
read one of them now reads `num_states`/`num_mobj_types` instead
(`P_SetMobjState`'s cycle-detection tables and `P_SpawnMobj`'s doomednum hash in
`p_mobj.c`, FraggleScript's `spawnobject` bounds check in `t_func.c`, and the
several DEHACKED bounds checks in `d_deh.c` below).

One deliberate simplification versus Woof: Woof's index-translation tables are
hash maps (`src/m_hashmap.c`), because DSDHacked patches can reference
arbitrarily large frame/thing numbers and it wants O(1) lookups. SMACK has no
hash table of its own and this only runs while parsing DEHACKED patches at
startup, so `d_dsdh.c` uses a linear-scan array instead — simplest, and plenty
fast for the handful of extended entries any real patch defines.

**This also fixes two pre-existing memory-corruption bugs**, found while wiring
up the translation: `deh_procThing` (the DEHACKED `Thing` block) had *no* bounds
check at all before writing `mobjinfo[indexnum]`, and `deh_procFrame` (the
`Frame` block) printed a warning for an out-of-range index but then wrote
`states[indexnum]` anyway — so on a stock (pre-DSDHacked) build, any DEHACKED
patch defining a `Thing`/`Frame` numbered past the end of the original table
silently corrupted memory beyond it. Both now route through
`DSDH_ThingTranslate`/`DSDH_StateTranslate`, which is exactly the fix and gets
the index a real, valid slot instead. A third, unrelated out-of-bounds read in
`deh_procPointer` (a loop over the ~100-entry `deh_bexptrs[]` bounded by
`NUMSTATES`, ~970) was fixed in passing, bounded by its own terminating entry
like every other walk of that array.

**Scope**: states and things only, per DSDHacked's core feature and this fork's
need — sprite names (`sprnames[NUMSPRITES]`) and the sound table
(`S_sfx[NUMSFX]`) remain fixed-size, so a patch that both extends the state
table *and* needs new sprite/sound lump names of its own is only partly
supported: the extra states work, but referencing a sprite/sound number beyond
the original tables will not.

### MBF21: line flags

- `ML_BLOCKLANDMONSTERS` (bit 12) and `ML_BLOCKPLAYERS` (bit 13) added to
  `doomdata.h` and enforced in `PIT_CheckLine` (`p_map.c`), mirroring Woof
  (`src/doomdata.h:217`, `src/p_map.c:419,427`): players are blocked by
  `ML_BLOCKPLAYERS`, and monsters without `MF_FLOAT` by `ML_BLOCKLANDMONSTERS`.
  Bouncing and missile objects pass through both, as with the older flags.
- Linedef flags are now read through `(unsigned short)` in `p_setup.c` (as Woof
  does at `src/p_setup.c:388`). `SHORT()` returns a *signed* short, so a linedef
  with bit 15 set sign-extended into the `int` and turned on every high bit —
  which would have made those lines block players and land monsters wholesale.
- Caveat: SMACK has no complevel system, so these two bits are always honoured.
  Woof gates them on `mbf21` and additionally masks flags to `0x1FF` on a line
  with the reserved bit set (`comp_reservedlineflag`). That masking is *not*
  ported here, because SMACK treats bit 9 as Boom’s `ML_PASSUSE`.

## 2026-08-21

### Resolution is a runtime value (groundwork)

- `SCREENWIDTH`/`SCREENHEIGHT` were compile-time `#define`s; they are now
  variables, still 320x200 base doubled by `hires`. **Nothing about what the
  game draws changes** -- this is the step every later move toward arbitrary
  resolutions and widescreen depends on, done on its own so it stays
  reviewable.
- New `BASE_WIDTH`/`BASE_HEIGHT` name the fixed 320x200 space HUD and menu
  layout is expressed in, which `V_DrawPatch` scales up. Static widget
  positions use those; the live resolution uses SCREENWIDTH/SCREENHEIGHT. The
  distinction did not need naming while both were the same constant.
- Of 453 uses, only four places actually needed changing -- one array bound
  (`f_wipe.c`) and three static initialisers that are no longer constant
  expressions. gcc caught all three; MSVC accepted them silently.

## 2026-08-20

### UMAPINFO

- `p_umapinfo.c` parses the UMAPINFO lump, the de-facto standard modern
  megawads use to describe their own maps. SMMU already had an equivalent of
  its own -- `p_info.c` reads the same fields out of the map marker lump -- so
  this feeds those globals rather than adding a second level-info system, and
  is applied after them so a wad's UMAPINFO wins.
- **Applied today:** `levelname`, `levelpic`, `next`, `par`, `music`,
  `skytexture`, `intertext`, `interbackdrop`.
- **Parsed but not yet acted on:** `nextsecret` (p_info has no secret-exit
  field to hang it on), `label`, `endgame`, `nointermission`. `bossaction`,
  `endpic` and anything from a later revision are skipped cleanly -- unknown
  keys must never stop a wad loading.
- Every UMAPINFO in load order is parsed, so a later pwad overrides an earlier
  one per key, and a wad loaded at runtime re-parses.

### CI

- GitHub Actions builds all three toolchains on every push: Linux/gcc,
  Windows/MSVC and Windows/mingw-w64. The Linux job is the point -- development
  happens on Windows, where SDL3 is absent and `Makefile.sdl3` cannot be run, so
  shared-source changes had been going in unverified. It builds release and
  debug, smoke-tests headlessly (no IWAD present, so it must exit with the IWAD
  error rather than a signal) and runs the source-list drift check.
- The MSVC job greps its own log for C4013 and fails on a hit -- the implicit
  declaration warning that catches a pointer-truncating call under LLP64.
- Two real bugs surfaced on the very first run: MSYS2's gcc defaults to C23,
  where `false`/`true` are keywords and so cannot name enumerators, breaking the
  1999 `typedef enum {false,true} boolean;` (fixed with a C23 branch that keeps
  the 4-byte enum, plus `-std=gnu17` pinned in both gcc makefiles); and SDL3's
  source build wanted the full X11 dev set, now configured headless with
  `-DSDL_UNIX_CONSOLE_BUILD=ON`.

### Frame interpolation (`uncapped`, on by default)

- Interpolates the view and sprites between the 35 Hz game tics, so motion is
  smooth at whatever rate the loop draws at. The loop already drew unbounded --
  that is why the process pegs a core -- but every frame between tics showed
  identical world state, so it looked like 35 fps regardless.
- Display-only: positions are snapshotted once per tic and the blended values
  never feed back into the playsim, so demos and netgames are unaffected. The
  tic rate itself is untouched, which it has to be: `TICRATE` is baked into
  every movement constant, weapon and monster timing, and the demo format is
  one ticcmd per tic.
- Interpolation is suppressed across discontinuities the way Woof and Crispy do
  it -- spawn and teleport reset a thing's previous position, and an
  implausible delta snaps rather than sweeps. Getting that wrong was a
  ~40%-of-runs segfault; see `LEGACY_FIXES.md`.
- Measured: 0 crashes in 16 runs, and 16/16 frame pairs 9 ms apart differ
  (0/14 before), i.e. distinct images really are drawn inside one 28.6 ms tic.

## 2026-08-13

### Features menu: jump, hit indicator, end-game confirmation

- **Options → game options → features** is new and replaces the "end game" entry
  there; end game moved inside it.
- **Jump** (`jump`, off by default; key defaults to `e`, bindable under
  Options → key bindings). There was no free bit left in `ticcmd_t.buttons` --
  `BT_WEAPONMASK` here is `(8+16+32+64)` for the SSG -- so the impulse rides in a
  new `jump` field which, like the existing `updownangle`, is deliberately **not**
  serialized: `G_WriteDemoTiccmd` stores only forwardmove/sidemove/angleturn/
  buttons. The demo format is therefore unchanged, playback clears the field, and
  jumping is refused in netgames and demos.
- **Hit indicator** (`hitindicator`, on by default), ported from BuddyDoom's
  damage ring: a red arc around the crosshair pointing at where damage came from,
  bearing = attacker angle − viewangle so it rotates as you turn. It only
  timestamps an arc from `P_DamageMobj` and draws in `HU_Drawer`, touching no
  playsim state and no RNG.
- **End game confirmation** (`endgame_style`): `vanilla` keeps the "are you sure?"
  prompt, `skip message` ends immediately.

### Window size and fullscreen are remembered

- New cvars `v_width`, `v_height` and `v_fullscreen`, saved in `ID0/smack.cfg`.
  Resize the window and it comes back that size next launch; the window is
  created fullscreen if the flag is set.
- **Fullscreen** is new: Options → video → "fullscreen", `v_fullscreen` at the
  console, or **Alt+Enter**, which is swallowed so it never reaches the game as a
  "use" press.
- Command-line sizing (`-geom`, `-2`/`-3`/`-4`, `SMACK_SCALE`) still outranks the
  saved size but is deliberately one-shot — it no longer rewrites the config. The
  exception is a config with no size yet, where the size used is the only one the
  player has seen and so is worth storing.
- The resize handler skips recording while fullscreen, so the monitor size never
  becomes the remembered windowed size; leaving fullscreen restores the window.

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
