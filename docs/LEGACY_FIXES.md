# Legacy fixes — making SMMU 3.21 (1999) build and run on 2020s hardware

Source-backed running log of the portability and modernization work in this SMMU
tree. SMMU ("Smack My Marine Up") is an MBF-based DOOM port by Simon Howard; this
fork brings the 1999 DOS/DJGPP codebase up on **64-bit Linux with an SDL3 backend**
(`linux/`, built via `Makefile.sdl3`) and adds a few modern conveniences.

Format per entry: symptom → root cause → fix → files. An entry can describe a bug
that is already fixed; still-open limitations are called out as such.

**Because SMMU is MBF-derived, it already carries Lee Killough's 1998 64-bit and
limit-removing cleanups.** An audit of this tree against BuddyDoom's
`docs/LEGACY_FIXES.md` (a heavier ID24/limit-removing fork) found **no live legacy
bug** in the shared portability areas — see §9. The work below is mostly the SDL3
port, the hi-res/HUD features, and a handful of genuine fixes.

**Last audit:** 2026-07-25.

---

## 1. Build / tooling (`Makefile.sdl3`)

The original `makefile` is the DOS/DJGPP build (Allegro, `.s` asm) and is dead —
kept only for reference. `Makefile.sdl3` is the live Linux/SDL3 build.

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| Link fails: `undefined reference to VERSION / version_name / version_date` | `OBJS` never listed `version.o` (the DJGPP makefile linked it via a separate rule) | add `$(O)/version.o` to `OBJS` | `Makefile.sdl3` |
| `make debug` dies on `djgpp/i_system.c` | the `debug` target re-invoked `$(MAKE)` **without `-f Makefile.sdl3`**, falling through to the dead DOS makefile | `$(MAKE) -f Makefile.sdl3 MODE=DEBUG` | `Makefile.sdl3` |
| **From-scratch build fails: `multiple definition of demo_insurance / weapon_recoil / …`** | 1999-era **tentative-definition globals** (`int demo_insurance;` with no `extern`, in several `.c` files). Modern gcc defaults to `-fno-common`, which makes these a link error. (Incremental builds masked it by reusing `.o` files compiled elsewhere; `make clean` exposes it.) | `-fcommon` | `Makefile.sdl3` |
| Latent `-O2` miscompiles | the engine type-puns constantly (comparing 4-char lump names as ints, etc.); default strict aliasing lets `-O2` reorder those wrongly. SMMU happens to avoid the exact classic pun (it uses `strncasecmp`/byte-XOR for names), but the risk is real | `-fno-strict-aliasing` (mandatory for the whole DOOM family) | `Makefile.sdl3` |
| Editing a `.h` didn't recompile the `.c` files that include it → stale objects, confusing behaviour | the make rules tracked **no header dependencies** | `-MMD -MP` per compile + `-include $(OBJS:.o=.d)` | `Makefile.sdl3` |
| Running `./smack` used a stale binary | nothing copied the freshly built exe next to its WADs | `all: $(EXE) run`; the `run` target copies the binary and refreshes the `run/smack.wad` symlink every build | `Makefile.sdl3` |

The binary resolves its data dir from `argv[0]` (`D_DoomExeDir`), so it must sit
beside its WADs — hence the `run/` runtime image.

---

## 2. 64-bit (LP64) pointer assumptions

The one **real** 64-bit bug found in this tree (the rest of §1-class hazards are
absent — see §9):

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| **SIGSEGV at startup** in `D_ProcessWadPreincludes` (`isspace(*s)`) right after `M_LoadDefaults` | `M_LoadDefaults` seeded string config defaults with `*dp->location = (int) strdup(...)` — truncating the 64-bit heap pointer to 32 bits. The `wad_files[]`/chat-macro/etc. pointers came back mangled | write the full pointer for string defaults: `*(char **) dp->location = strdup(...)` (the idiom already used elsewhere in the file) | `m_misc.c` (`M_LoadDefaults`) |

Everything else in the classic 64-bit checklist is already clean here (verified,
not assumed): the WAD handle is a POSIX `int` fd (not a truncated `FILE*`), the
on-disk `maptexture_t` uses a 4-byte `pad` (not a `void **columndirectory`),
pointer arrays allocate with `sizeof(*p)`, `z_zone.c` is a modern malloc-backed
rewrite with a clean NULL owner-test, and `boolean` is a 4-byte `enum`. See §9.

---

## 3. SDL3 backend (`linux/i_video.c`, `linux/i_sound.c`, `linux/i_main.c`)

The DOS backend (Allegro + `.s` blitters + inline MIDI) is non-portable. The Linux
backend is reimplemented on SDL3.

- **Video** (`i_video.c`): the 8-bit palettised DOOM framebuffer is converted to an
  RGBA8888 streaming texture and stretched to the window with nearest-neighbour
  sampling. `SMACK_SCALE=N` magnifies the window.
- **SFX** (`i_sound.c`): originally silent stubs. Now a real software mixer — parses
  DOOM's 8-bit DMX sound lumps, resamples to 44.1 kHz, applies pitch and the classic
  x² stereo pan law, mixes up to 128 voices into an SDL3 audio stream via a pull
  callback, guarded with `SDL_LockAudioStream`.
- **Music** — full **OPL3 (Adlib/SoundBlaster) synthesis**, the authentic DOOM sound
  with no external soundfont. Ported Nuked-OPL3 (`opl3.c`), a passive GENMIDI→OPL
  voice player (`i_opl.c`), and a MUS+MIDI sequencer (`i_mus.c`). `I_InitMusic` loads
  the IWAD's `GENMIDI` patches and inits the synth at 44.1 kHz; `I_RegisterSong`/
  `PlaySong`/`StopSong` drive it; the SDL3 callback renders the OPL stream and mixes
  it over the SFX (scaled by `snd_MusicVolume`). See §11.

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| **No sound at all** even after the mixer was written | `snd_card` stayed `0`, and `S_StartSfxInfo` early-returns on `!snd_card` — so `I_StartSound` was never even reached | set `snd_card = 1` on successful `I_InitSound` | `linux/i_sound.c` |
| **No music at all** even after the synth was written | `I_InitMusic` was **never called** anywhere (and `mus_card` stayed 0, so the `S_*` music paths early-returned) | call `I_InitMusic` from `S_Init` (gated on `!nomusicparm`); it sets `mus_card` | `s_sound.c`, `linux/i_sound.c` |
| **Whole image renders at ~¼ brightness** ("very dark compared to other ports") | `I_SetPalette` shifted gamma-corrected values `>> 2` — correct for the VGA 6-bit DAC (0–63), wrong for SDL's 8-bit RGB (0–255) | use the full 8-bit `gammatable` value directly | `linux/i_video.c` |
| Mouse stayed captured in menus | relative-mouse mode was enabled whenever `usemouse`, never released | `I_UpdateGrab()` each tic: grab only during active gameplay; release (cursor back, motion suppressed) when `menuactive`/`consoleactive`/`paused` | `linux/i_video.c` |

---

## 4. Hi-res by default; lowres removed

SMMU's renderer keys off `hires` (a **shift**, 0 or 1) via `SCREENWIDTH<<hires`, so
`hires=1` is a true 640×400 internal framebuffer (not just window scaling). This
port makes hi-res the default and the **only** mode.

- `hires = 1` fixed (`linux/i_video.c`); the SDL3 backend is now hires-aware
  (texture/surface/blit/`I_ReadScreen` use `SCREENWIDTH<<hires` × `SCREENHEIGHT<<hires`).
- The "video mode" (lores/hires) menu toggle, the `SMMU_LORES` env hatch, and the
  lores branch in `V_Init` were removed. `V_Init` always allocates the 640×400
  buffers. (`mn_menus.c`, `v_misc.c`, `linux/i_video.c`)
- The base-320-coords + `<<hires` model is applied consistently, so 640×400 is
  genuinely correct — notably `visplane_t.top/bottom` are already `unsigned short`
  (they hold screen rows up to 400 > 255), and `scaledviewwidth`/`scaledviewheight`
  stay in base units. (`r_defs.h`, `r_main.c`)

---

## 5. Fullscreen HUD & the unified screen-size control

One `screensize` control (cvar `screensize`, the menu "screen size" slider, range
**0–11**) now drives the whole progression:

| screensize | result |
|---|---|
| 0–7 | windowed 3D view + status bar |
| 8 | fullscreen + classic text overlay |
| 9 | fullscreen + **GZDoom-style graphical HUD** (`HU_DrawFullHUD`, big corner numbers) |
| 10 | the same HUD at **50%** (`V_DrawPatchUnscaled` = 1× vs `V_DrawPatch` = 2× hires) |
| 11 | the **vanilla status bar scaled to 50%**, centred (`ST_DrawScaled`, aidoom-style) |

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| **Renderer SIGSEGV in `R_RenderSegLoop`** at screensize 9/10 (release only; `-O0` masked it) | `p_setup.c` calls `R_SetViewSize(screenSize+3)` at level load → block 12 → `scaledviewwidth = 12*32 = 384 > 320` → wall-render buffer overrun | clamp in one place: `R_ExecuteSetViewSize` treats any block ≥ 11 as fullscreen (`if (setblocks >= 11)`) | `r_main.c` |
| A new HUD style (4/5) set in the config was silently ignored | the config-defaults table (`m_misc.c`) capped `hud_overlaystyle` at `{0,3}` — separate from the console var's range — so out-of-range values were rejected on load | widen the range in **both** places | `m_misc.c`, `hu_over.c` |
| 50% scaled bar drew only the `%` signs, no numbers/face | `ST_Drawer` clears `st_statusbaron` in fullscreen, and the widgets gate on it | force `st_statusbaron = true` inside `ST_DrawScaled` before the capture | `st_stuff.c` |

Text-overlay styles (0–3) remain selectable via cvar `hu_overlay` (HUD settings →
"display type") for the screensize-8 fullscreen overlay.

---

## 6. Textured automap (aidoom-style)

`AM_drawFlats` (`am_map.c`, cvar `automap_textured`, default on; Options → automap →
"textured display") fills each **explored** subsector's floor area with its floor
flat, sampled per pixel and light-shaded. One `R_PointInSubsector` BSP descent per
4×4 block; reveal follows `ML_MAPPED` (or IDDT/computer-map). Uses
`firstflat + flattranslation[pic]` for the flat and `colormaps[0] + cm*256` for
shading. Cvar registered in `am_color.c`, config default in `m_misc.c` (persists).

---

## 7. Key bindings menu

SMMU 3.21 shipped a dead Options entry: `{it_info, "key bindings", "mn_keybindings"}`
— a non-selectable label pointing at a command that never existed. There was **no
key-binding UI** at all.

**Fix** (`mn_menus.c`): built `menu_keybindings` (populated at startup by
`MN_InitKeyBindings`) listing the core actions with their live key names. Selecting a
row runs `mn_bindkey N`, which installs `binding_widget` — a `menuwidget_t` "press a
key" capture prompt (same `current_menuwidget` mechanism as `mn_misc.c`'s
`popup_widget`); the next keypress is written to the corresponding `key_*` variable
(ESC cancels). The Options entry became a real `it_runcmd`. Bindings persist because
`key_*` are config variables.

---

## 8. Renderer hardening

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| A pathological seg-dense view could **silently corrupt memory** | `openings[]` is a fixed static array (`MAX_SCREENWIDTH*MAX_SCREENHEIGHT`); the BSP renderer holds pointers into it across the frame so it can't be made dynamic, but the writes in `R_StoreWallRange` were **unchecked** (no realloc, no `I_Error`) | bounds-check each `lastopening += …` and `I_Error("openings overflow")` instead of overrunning | `r_segs.c`, `r_plane.c/.h` (`MAXOPENINGS` exposed) |

---

## 9. Audit vs. BuddyDoom `LEGACY_FIXES.md` — what SMMU already has

A section-by-section audit of this tree against the heavier BuddyDoom/ID24 fork's
fix log. **SMMU is MBF-based, so nearly everything is native killough/MBF code**, not
a port-specific patch. Summary:

**Already present / handled (verified in source):**

- **64-bit (§1 there):** WAD handle is an `int` fd; no `(int)ptr` colormap
  alignment; `maptexture_t` uses a 4-byte pad; pointer arrays use `sizeof(*p)`;
  `z_zone.c` rewritten with a clean owner-test; `boolean` is 4-byte `enum`.
- **Hi-res (§3):** `visplane_t.top/bottom` are `unsigned short`; base-320 + `<<hires`
  applied consistently. 640×400 is correct, not accidental.
- **Buffers (§4):** zone heap is unbounded malloc (no 6 MB cap); `screens[4]`
  exact-fit; `MAXSEGS = MAX_SCREENWIDTH/2+1` (321); `drawsegs` grows dynamically.
- **Vanilla crash bugs (§16):** spechit / intercepts dynamic; **visplanes are
  killough-hash dynamic** (no cap); ghost monsters, tutti-frutti, invulnerability-sky
  all fixed (compat-gated); vissprites dynamic.
- **Generalized thinkers (§11):** complete native Boom support
  (`p_doors.c`/`p_floor.c`/`p_ceilng.c`).
- **Modern-texture limits (§12/§14):** 32-bit `texturecolumnofs`; no >64 KB cap;
  `dc_texheight` (no 128-row `&127` wrap); no `MAXSWITCHES` cap (growable list).
- **Medikit "…REALLY need!" message (§15):** reachable (threshold bumped to `<50`
  after heal — equivalent to the sample-before-heal fix).

**Deliberately KEPT for demo/map compat (do NOT "fix"):** the vanilla stair-builder
bug (height-bump-before-check + `secnum` clobber), the Pain-Elemental 20-lost-soul
cap, and wallrunning — all present and gated by MBF's `comp[]` flags, which are
serialized into demo headers, so they stay deterministic. **Changing any playsim
logic desyncs demos and netgames.**

**Not applicable (BuddyDoom-only subsystems):** the AI buddy / voice / Director,
DSDHacked/Legacy-of-Rust thing-number packing, Heretic/Hexen mobjtypes, GZDoom PNG
sprite WADs, and the `buddydoom.wad` late-load `lumpcache` realloc — none of these
exist in SMMU.

**Deferred limitations (present, but out of scope for stock IWADs):**

- ~~**Tall single-patch textures (>254 rows, DeePsea encoding)**~~ — **fixed**, see §13.
- **Savegame index swizzle:** `p_saveg.c` reads `state/player/sector/line` refs back
  through `(int)` rather than `intptr_t`. Works today (the indices are small,
  zero/sign-extended, and the save buffer is dynamic); the critical mobj refs already
  use `(size_t)`. Hardening-for-parity only, not a live bug.

---

## 10. WiggleHack II — tall-wall texture shimmer

Vanilla clamps the wall texture scale to a fixed `64*FRACUNIT` with 12-bit height
precision (`HEIGHTBITS`), tuned for 128-tall walls. On taller walls — and at hi-res
— that fixed precision makes wall textures visibly **"wiggle"/shimmer** vertically as
the view moves. Ported **WiggleHack II** (Kurt Baumgardner & Andrey Budko, via
Woof/prboom): the fixed-point precision and the scale clamp are chosen **per wall**
from the sector height.

| Symptom | Root cause | Fix | Files |
|---|---|---|---|
| Tall walls shimmer/wobble vertically as you move | fixed 12-bit `HEIGHTBITS` + fixed `64*FRACUNIT` scale clamp, tuned for 128px walls | `R_FixWiggle(frontsector)` (keyed on sector height) sets a per-wall `heightbits`/`invhgtbits` and `max_rwscale` from an 8-entry `scale_values` table; `R_ScaleFromGlobalAngle` clamps to `max_rwscale`; `topfrac`/`bottomfrac`/`pixhigh`/`pixlow` widened to `int64_t` and the frac math shifts by the runtime `invhgtbits` | `r_segs.c`, `r_main.c`, `r_defs.h` (`sector_t.cachedheight`/`scaleindex`) |

Purely a renderer-precision change — no playsim effect, so it's demo/netgame-safe. If
tall walls ever shimmer again, check `R_FixWiggle`/`max_rwscale` in `r_segs.c`.

---

## 11. OPL3 music synthesis

The DOS backend played music through AWE32/EMU8K hardware MIDI (`djgpp/emu8kmid.c`)
and a MUS→MIDI converter (`djgpp/mmus2mid.c`) — neither portable, neither built by
`Makefile.sdl3`. Music was fully silent (stub `I_*Song`/`I_*Music`, `mus_card = 0`,
and `I_InitMusic` never even called). This port adds authentic Adlib/OPL sound with
**no external dependency or soundfont**, driving the same SDL3 audio stream as the
SFX:

- **`opl3.c` / `opl3.h`** — the Nuked-OPL3 cycle-accurate OPL2/OPL3 emulator.
  `OPL3_Reset(rate)` + `OPL3_GenerateStream` render (and internally resample from the
  chip's native 49716 Hz) to any target rate — here 44.1 kHz to match the SFX stream.
- **`i_opl.c` / `i_opl.h`** — a *passive* GENMIDI→OPL voice player. Parses the IWAD's
  `GENMIDI` lump (128 instruments + 47 percussion, `#OPL_II#` header) and exposes
  `OPL_Music_NoteOn/Off/Program/ChannelVolume/PitchBend` + `OPL_Music_Render(out, n)`.
- **`i_mus.c` / `i_mus.h`** — a sequencer for both native DOOM **MUS** lumps and raw
  **MIDI** (`MThd`), timed at 140 Hz; `MUS_Render` fires events then pulls OPL audio
  for the gap between them.

Integration (`linux/i_sound.c`): `I_InitMusic` calls `MUS_Init` (loads `GENMIDI`,
inits the synth) and sets `mus_card`; `I_RegisterSong` derives the exact lump length
from the MUS/MIDI header (so it needs no `W_LumpLength` plumbing) and calls
`MUS_Register`; the audio callback renders `MUS_Render` into a scratch buffer and
mixes it over the SFX, scaled by `snd_MusicVolume`. All state changes are guarded
with `SDL_LockAudioStream` (the sequencer runs on the audio thread). `MUSRATE` in
`i_mus.c` was changed from aidoom's 11025 to **44100** to match `OUT_FREQ`.

---

## 12. Renderer-correctness batch (Woof parity)

A survey of Woof's software renderer against SMMU's found it already at parity for
most things (dynamic visplanes/drawsegs/vissprites, tutti-frutti, ghost monsters,
Boom 271/272 sky, `dc_texheight`, WiggleHack II §10). These smaller correctness /
crash-safety fixes were still missing and were ported (all renderer-only →
demo/netgame-safe):

| Symptom | Fix | Files |
|---|---|---|
| Segs/walls flicker or leave HOM on very large maps / geometry far from the origin | `R_PointToAngleCrispy` + `SlopeDivCrispy` (overflow-safe 64-bit angle) at the BSP clip call sites; the playsim keeps vanilla `R_PointToAngle` | `tables.c`, `r_main.c`, `r_bsp.c` |
| Sprites clipped at the wrong Y / garbage when tall or drawn at close range (large `spryscale`); tall (DeePsea) sprite posts | `sprtopscreen` → `int64_t`; `R_DrawMaskedColumn` tracks cumulative tall-post topdeltas; removed the 32-bit truncation in `R_RenderMaskedSegRange` | `r_things.c/.h`, `r_segs.c` |
| Sprites jump/flicker/vanish near the screen edge (`tx*xscale` / `tz<<2` 32-bit overflow) | `FixedMul64` for the projection; overflow-safe side cull; `vx1` guard for degenerate 1-column sprites | `m_fixed.h`, `r_things.c` |
| Garbage texture column looking down a long wall (`finetangent[]` index overflow) | mask the index with `0xFFF` | `r_segs.c` |
| OOB read / garbage columns on flipped or screen-edge sprites in the **release** build | `R_DrawVisSprite` clamps the column (`continue`/`break`) instead of a RANGECHECK-only `I_Error` | `r_things.c` |
| Flicker / nondeterministic draw order of sprites at identical distance | stable merge sort (`>=` + reverse-fill), matching Woof | `r_things.c` |
| Crash on a malformed/hostile `TEXTURE1` (out-of-range patch index) | bounds-check the mappatch index against `nummappatches` | `r_data.c` |

Deliberately NOT ported (features / out of scope for a deterministic 640×400 4:3
port): brightmaps, sky-color drawer / SKYDEFS, non-power-of-2 textures, truecolor,
uncapped-framerate interpolation, widescreen, the `R_MapPlane` precision rewrite
(sub-pixel), drawseg-bucketing / `solidcol` rewrite (perf/architectural).

---

## 13. Tall (DeePsea, >254-row) textures

The last known render gap (§9), now closed. The 1993 texture composer returns
single-patch columns **raw** from the patch lump; a >254-row texture is stored as
multiple posts (DeePsea: a post whose `topdelta` ≤ the previous one is a *cumulative*
continuation), so reading it raw makes `R_DrawColumn` interpret post headers as pixels
→ scrambled bands below ~row 254 (e.g. Legacy of Rust `ZZZGATE*`). Stock
DOOM/DOOM2/Plutonia/TNT are ≤128 tall and were unaffected.

Ported Woof's fix (`r_data.c`): a separate **flat opaque composite** buffer
(`texturecomposite2`/`texturecolumnofs2`, `colofs2[x] = x*height`) is built for **every**
texture and returned by `R_GetColumn` for 1s walls/flats; the posted composite (with a
new **cumulative-topdelta reconstruction** past row 254) is returned by the new
`R_GetColumnMasked` for 2s mid-textures. `R_DrawColumnInCache` tracks cumulative
topdeltas. The relative logic is a no-op for ≤254-tall content, so stock textures render
**identically** (verified on DOOM2 MAP01). Files: `r_data.c`, `r_data.h`, `r_segs.c`.

---

## 14. Long-wall wobble (fixed-point precision)

On long walls in large maps, vanilla's fixed-point `R_PointToDist` /
angle-from-`rw_angle1` math loses precision, so the wall texture visibly shears /
mis-aligns as the view moves. Ported Woof's fix:

- **`P_SegLengths`** (`p_setup.c`, called from `P_SetupLevel`) pre-computes, per seg,
  a precise length (`r_length = sqrt(dx²+dy²)/2`) and angle (`r_angle`, via the
  overflow-safe `R_PointToAngleCrispy`, falling back to the BSP angle if it differs by
  >~30°). Render data only → demo/netgame-safe.
- **`R_StoreWallRange`** (`r_segs.c`) computes `rw_distance` (perpendicular distance)
  and `rw_offset` (along-seg projection) from **int64** cross/dot products of the seg
  and view vectors, clamped to `fixed_t`, instead of the trig path. `seg_t` gained
  `r_length`/`r_angle` (`r_defs.h`).

The result is identical for short walls (verified pixel-identical on DOOM2 MAP01) and
steady on long ones.

---

## Config persistence

All console/config variables (screen size, HUD style, key bindings, automap options
including the textured toggle, gamma, sound/music volume, …) are written to
`run (next to the binary)/smack.cfg` on clean exit via `M_SaveDefaults` (called from `I_Quit`, an
`atexit` handler). **The window size is NOT persisted** — it derives from
`SMACK_SCALE` each launch.

---

## How to spot the next one

- **From-scratch link fails with `multiple definition`** → a tentative-definition
  global; needs `-fcommon` (§1). Incremental builds hide it.
- **A startup/render glitch only at `-O2`** → strict aliasing; `-fno-strict-aliasing`
  is mandatory (§1).
- **"It built but behaves weird after I edited a header"** → should no longer happen
  (`-MMD -MP` now tracks header deps, §1); if it recurs, check the `.d` include.
- **Crash/garbage that scales with resolution, or a clamped/banded 3D view** → a
  place that baked in 320/200 or a `byte` that should hold a 640×400 screen row (§4);
  `visplane_t.top/bottom` were the classic one (already fixed).
- **`R_StoreWallRange: openings overflow`** → the one renderer array that is a hard
  bound-checked cap, not dynamic (§8). A genuinely pathological view; raise
  `MAXOPENINGS` if a real map hits it.
- **Everything renders too dark** → the palette 6-bit `>> 2` shift is back (§3).
- **A modern PWAD's tall wall texture is garbage below ~row 254** → the DeePsea
  tall-texture handling (§13); should already be fixed — check `R_GetColumn`/
  `R_GenerateComposite` (the flat `texturecomposite2`).
- **Stairs build to the wrong height / a second staircase off one switch is wrong**
  → that's the *vanilla* stair-builder bug, kept on purpose (§9). Not yours to fix.
