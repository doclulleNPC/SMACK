//
// Force-included into every translation unit of the MSVC build (cl /FI).
//
// It carries only the things that must be visible everywhere and that the
// engine's own headers do not provide -- deliberately kept tiny, so the 1999
// sources stay free of MSVC-specific clutter. Anything a single file needs goes
// in that file instead.
//
// MSVC build only. mingw-w64 and Linux never include this.
//
#ifndef __SMACK_MSVC_COMPAT_H__
#define __SMACK_MSVC_COMPAT_H__

// PATH_MAX: used for the fixed-size path buffers in d_main.c, g_game.c,
// mn_menus.c and r_data.c. MSVC spells it _MAX_PATH (260) in <stdlib.h>.
#include <stdlib.h>
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

// The engine uses the POSIX spelling of the case-insensitive compares.
#include <string.h>
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

// S_ISDIR: used by FindIWADFile() in d_main.c. MSVC's <sys/stat.h> has the
// _S_IF* bits but not the POSIX test macros. Without this the compiler takes
// S_ISDIR for an implicit function and it fails at link time.
#include <sys/types.h>
#include <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

#endif // __SMACK_MSVC_COMPAT_H__
