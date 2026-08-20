// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: doomtype.h,v 1.3 1998/05/03 23:24:33 killough Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Simple basic typedefs, isolated here to make it easier
//       separating modules.
//
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
// Fixed to use builtin bool type with C++.
#ifdef __cplusplus
typedef bool boolean;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
// C23 made false/true keywords, so they can no longer *name* enumerators.
// The tree still uses them as values, which is fine: they are bool constants
// that convert to the enum. Keep an enum rather than moving to C23 bool --
// boolean is 4 bytes here and that size is baked into the struct layouts the
// savegame code writes out.
typedef enum {boolean_false = 0, boolean_true = 1} boolean;
#else
typedef enum {false, true} boolean;
#endif
typedef unsigned char byte;
#endif

// This used to be #include <values.h>, a legacy SVR4/glibc header that neither
// mingw-w64 nor Cygwin ships. The engine only ever uses MAXINT / MININT /
// MAXSHORT out of it, and glibc's values.h defines those from <limits.h>
// anyway, so derive them here instead: same constants everywhere, no
// platform conditional, and the playsim is unaffected.
#include <limits.h>
#ifndef MAXINT
#define MAXINT          INT_MAX
#endif
#ifndef MININT
#define MININT          INT_MIN
#endif
#ifndef MAXSHORT
#define MAXSHORT        SHRT_MAX
#endif
#ifndef MINSHORT
#define MINSHORT        SHRT_MIN
#endif
#define MAXCHAR         ((char)0x7f)
#define MINCHAR         ((char)0x80)
#endif

//----------------------------------------------------------------------------
//
// $Log: doomtype.h,v $
// Revision 1.3  1998/05/03  23:24:33  killough
// beautification
//
// Revision 1.2  1998/01/26  19:26:43  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
