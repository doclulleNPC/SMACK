// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Locating IWADs installed by Steam -- see d_iwad.c.
//
//-----------------------------------------------------------------------------

#ifndef __D_IWAD__
#define __D_IWAD__

// Number of existing Steam directories that might hold an IWAD, and the n'th
// of them (in search-priority order). Enumerated once, on first use.
int         D_NumSteamIWADDirs(void);
const char *D_SteamIWADDir(int n);

// Search those directories for the first of `names` that exists, trying each
// name as given, lowercased and uppercased (Steam's classic packaging uses
// DOOM.WAD, which matters on case-sensitive filesystems). Names may begin with
// a '/' -- standard_iwads[] in d_main.c does. Returns a pointer to a static
// buffer, or NULL if nothing was found.
const char *D_SteamFindIWAD(const char *const *names, int nnames);

#endif
