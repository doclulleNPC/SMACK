// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// UMAPINFO support.
//
// The de-facto standard for modern megawads to describe their own maps: level
// names, progression, par times, music, sky. SMMU already had an equivalent of
// its own (p_info.c, which reads the same fields out of the map marker lump),
// so this parses UMAPINFO and feeds those same globals rather than introducing
// a second level-info system.
//
// Spec: https://doomwiki.org/wiki/UMAPINFO
//
//-----------------------------------------------------------------------------

#ifndef __P_UMAPINFO__
#define __P_UMAPINFO__

#include "doomtype.h"

// Parse every UMAPINFO lump present, latest wad winning. Safe to call again
// after loading a wad at runtime.
void P_LoadUMapInfo(void);

// Override the p_info globals for `mapname` from UMAPINFO, if it has an entry.
// Called after P_LoadLevelInfo has set the defaults. Returns true if applied.
boolean P_ApplyUMapInfo(const char *mapname);

#endif
