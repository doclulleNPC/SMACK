// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Frame interpolation.
//
// The game simulates at 35 Hz but the main loop already draws as fast as it
// can, so without this every frame between tics shows an identical world and
// motion looks like 35 fps. `fractionaltic` says how far we are through the
// current tic, and the renderer uses it to draw partway between a thing's
// previous and current position.
//
// Display-only: interpolated values never feed back into the playsim, and the
// previous-state fields are written once per tic by P_SaveInterpolationState.
// Demos and netgames are unaffected.
//
//-----------------------------------------------------------------------------

#ifndef __R_INTERP__
#define __R_INTERP__

#include "doomtype.h"
#include "m_fixed.h"
#include "tables.h"

struct mobj_s;

extern int     uncapped;        // cvar: interpolate between tics
extern fixed_t fractionaltic;   // 0..FRACUNIT through the current tic
extern int     interp_stamp;    // bumped per tic; see r_interp.c

// Sub-tic fraction from the platform clock (linux/i_system.c). Declared here
// rather than in i_system.h, which cannot include m_fixed.h -- m_fixed.h
// includes i_system.h, so fixed_t is not available there.
fixed_t I_GetTimeFrac(void);

// Whether it is safe and wanted to interpolate this frame.
boolean R_Interpolating(void);

// Save every thing's current position as "previous". Once per game tic.
void P_SaveInterpolationState(void);

// Forget a thing's previous position -- call after any discontinuous move.
void P_ResetInterpolation(struct mobj_s *mo);

void R_Interp_AddCommands(void);

// Blend from -> to by fractionaltic.
// A thing cannot legitimately move more than this in one tic (MAXMOVE is 30
// units; a lift or a heavy thrust is still far below 128). A larger delta means
// a discontinuity someone forgot to reset, so snap rather than sweep the thing
// across the level -- and rather than feed a wild coordinate to the projection
// maths, which is where that ends badly.
#define R_INTERP_MAXDELTA (128*FRACUNIT)

inline static fixed_t R_LerpFixed(fixed_t from, fixed_t to)
{
  fixed_t delta = to - from;

  if (delta > R_INTERP_MAXDELTA || delta < -R_INTERP_MAXDELTA)
    return to;

  return from + FixedMul(delta, fractionaltic);
}

// Angles are modular: subtracting two angle_t values wraps to the shortest
// signed delta, which is what we want to scale. Lerping the raw values instead
// would spin the long way round whenever the angle crosses zero.
inline static angle_t R_LerpAngle(angle_t from, angle_t to)
{
  return from + (angle_t) FixedMul((fixed_t)(to - from), fractionaltic);
}

#endif
