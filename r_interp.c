// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Frame interpolation -- see r_interp.h for what this is and why it is safe.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "d_player.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "r_interp.h"
#include "c_runcmd.h"

int     uncapped = 1;       // cvar (m_misc.c persists it)
fixed_t fractionaltic;

// Bumped once per game tic. Things carry the stamp they were last snapshotted
// on, and the renderer interpolates only those matching the current one.
//
// This deliberately does NOT use gametic: gametic is incremented *after* the
// tic runs, so a stamp taken during the tic never equals it at render time and
// nothing would ever interpolate. (It didn't -- that was the first bug here.)
// Starts at 1, never 0: P_SpawnMobj memsets a new thing, so its interpvalid
// is 0, and 0 must never match a live stamp. It did when this started at 0 --
// any frame drawn before the first tic then interpolated every thing from
// (0,0,0) toward its real position, which crashed the projection maths.
int     interp_stamp = 1;

boolean R_Interpolating(void)
{
  // Only in a running level. Menus, intermissions and the finale have no
  // previous state worth blending, and while paused or single-stepping the
  // "previous" tic never advances, so interpolating would drift the view.
  return uncapped && gamestate == GS_LEVEL && !paused && !singletics;
}

//
// P_ResetInterpolation
//
// Called wherever a thing moves discontinuously -- spawn, teleport -- so the
// renderer has no stale "previous" position to drag it from. Clearing the
// stamp is what actually suppresses interpolation; seeding old = current keeps
// the fields sane for anything that reads them anyway.
//
void P_ResetInterpolation(mobj_t *mo)
{
  mo->oldx = mo->x;
  mo->oldy = mo->y;
  mo->oldz = mo->z;
  mo->oldangle = mo->angle;
  mo->interpvalid = 0;
}

//
// P_SaveInterpolationState
//
// Called once per game tic, before the thinkers run, so every thing's current
// position becomes the "previous" one the renderer blends from.
//
// interpvalid is stamped with the tic it was taken on. A thing that has just
// been spawned, teleported or unarchived has no meaningful previous position,
// and drawing it sliding in from wherever it happened to be looks far worse
// than one stationary frame -- so the renderer only interpolates a thing whose
// stamp is current.
//
void P_SaveInterpolationState(void)
{
  thinker_t *th;
  int i;

  if (!uncapped)
    return;

  ++interp_stamp;

  for (th = thinkercap.next; th != &thinkercap; th = th->next)
    if (th->function == P_MobjThinker)
      {
        mobj_t *mo = (mobj_t *) th;
        mo->oldx = mo->x;
        mo->oldy = mo->y;
        mo->oldz = mo->z;
        mo->oldangle = mo->angle;
        mo->interpvalid = interp_stamp;
      }

  for (i = 0; i < MAXPLAYERS; i++)
    if (playeringame[i])
      players[i].oldviewz = players[i].viewz;
}

VARIABLE_BOOLEAN(uncapped, NULL, onoff);
CONSOLE_VARIABLE(uncapped, uncapped, 0) {}

void R_Interp_AddCommands(void)
{
  C_AddCommand(uncapped);
}
