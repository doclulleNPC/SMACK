//
// <unistd.h> for MSVC, which does not ship one.
//
// SMACK only needs a handful of POSIX file calls out of this header:
// access() (with F_OK/R_OK), and open()/read()/lseek()/close() in w_wad.c.
// The MSVC CRT provides all of them, spelled with a leading underscore in
// <io.h>; the unprefixed POSIX names are also declared there as long as
// _CRT_NONSTDC_NO_DEPRECATE is defined (Makefile.msvc / the .vcxproj do).
//
// This file exists only for the MSVC build. mingw-w64 and Linux have a real
// unistd.h and never see it, because msvc\compat is on the include path for
// the MSVC build alone.
//
#ifndef __SMACK_MSVC_UNISTD_H__
#define __SMACK_MSVC_UNISTD_H__

#include <io.h>         // access/_access, open, read, write, lseek, close
#include <direct.h>     // _mkdir, _getcwd
#include <process.h>    // _getpid, _exit
#include <stdlib.h>

// _access() mode bits. MSVC documents the numeric values but does not name
// them; these are the standard POSIX spellings. Windows has no execute
// permission bit, so X_OK degrades to a plain existence check.
#ifndef F_OK
#define F_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef X_OK
#define X_OK 0
#endif

#endif // __SMACK_MSVC_UNISTD_H__
