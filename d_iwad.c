// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Locating IWADs that were installed by Steam.
//
// SMACK's own search (FindIWADFile() in d_main.c) looks in the data directory,
// the current directory, the executable's directory and $DOOMWADDIR/$HOME.
// This adds a last resort: the copies of DOOM that Steam installs, including
// the 2024 "DOOM + DOOM II" re-release, on Windows, Linux and macOS.
//
// Deliberately self-contained -- it includes no engine headers beyond its own,
// because on Windows it needs <windows.h> for the registry, and winnt.h's
// SHORT/LONG typedefs collide with the macros of the same names in m_swap.h.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "d_iwad.h"

#define MAXDIRS   64
#define MAXPATHLN 512

static char  *steamdirs[MAXDIRS];
static int    numsteamdirs;
static int    steamdirs_built;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

static int DirExists(const char *path)
{
  struct stat sb;
  return path && *path && !stat(path, &sb) && (sb.st_mode & S_IFDIR);
}

static int FileExists(const char *path)
{
  struct stat sb;
  return path && *path && !stat(path, &sb) && !(sb.st_mode & S_IFDIR);
}

static void AddDir(const char *fmt, const char *a, const char *b)
{
  char buf[MAXPATHLN];
  int i;

  if (numsteamdirs >= MAXDIRS)
    return;

  if (b)
    snprintf(buf, sizeof buf, fmt, a, b);
  else
    snprintf(buf, sizeof buf, fmt, a);

  if (!DirExists(buf))
    return;

  for (i = 0; i < numsteamdirs; i++)     // no duplicates
    if (!strcmp(steamdirs[i], buf))
      return;

  steamdirs[numsteamdirs++] = strdup(buf);
}

// ---------------------------------------------------------------------------
// Steam library roots
// ---------------------------------------------------------------------------

// steamapps/libraryfolders.vdf lists every library Steam knows about, which is
// how games on a second drive are found. The format is Valve's KeyValues; all
// we need are the "path" entries, so this scans for them rather than pulling in
// a VDF parser. Paths in the file are backslash-escaped ("C:\\Steam").
static void AddLibrariesFromVdf(const char *steamroot,
                                void (*addlib)(const char *))
{
  char vdf[MAXPATHLN], line[MAXPATHLN * 2];
  FILE *fp;

  snprintf(vdf, sizeof vdf, "%s/steamapps/libraryfolders.vdf", steamroot);
  if (!(fp = fopen(vdf, "r")))
    return;

  while (fgets(line, sizeof line, fp))
    {
      char *p = strstr(line, "\"path\"");
      char *q, *out, path[MAXPATHLN];
      size_t n = 0;

      if (!p)
        continue;
      p += 6;
      while (*p && *p != '"')          // opening quote of the value
        p++;
      if (!*p)
        continue;
      p++;

      out = path;
      for (q = p; *q && *q != '"' && n < sizeof path - 1; q++, n++)
        {
          if (*q == '\\' && q[1] == '\\')   // "C:\\Steam" -> C:\Steam
            q++;
          *out++ = *q;
        }
      *out = 0;

      if (*path)
        addlib(path);
    }

  fclose(fp);
}

// Every Doom-bearing subdirectory of one Steam library, most specific first.
// Verified against a real install: the classic apps keep their IWADs under
// base/ (with the Final Doom and Doom II wads in subdirectories), while the
// 2024 re-release ships a flat rerelease/ folder. Both layouts appear under
// "Ultimate Doom" for the current Steam packaging, so all of them are tried.
static void AddLibrary(const char *lib)
{
  static const char *const subdirs[] =
  {
    // Classic DOSBox packaging first. These are the plain 1990s IWADs and are
    // what this engine was built for.
    "Ultimate Doom/base",
    "Ultimate Doom/base/doom2",
    "Ultimate Doom/base/plutonia",
    "Ultimate Doom/base/tnt",
    "Ultimate Doom/base/master/wads",
    "Doom 2/base",
    "Doom 2/finaldoombase",
    "Final Doom/base",
    "Doom Classic Complete/base",
    // BFG Edition keeps its wads one level deeper
    "DOOM 3 BFG Edition/base/wads",
    // The 2024 "DOOM + DOOM II" re-release (KEX): doom.wad, doom2.wad,
    // plutonia.wad, tnt.wad, nerve.wad, masterlevels.wad, id1.wad ...
    //
    // Deliberately LAST. Its IWADs carry ID24 extensions that this 1999-derived
    // renderer does not understand -- loading rerelease/doom2.wad crashes in
    // R_Init -- so when both packagings are installed (which is the normal case
    // now, since Steam ships them together under "Ultimate Doom") the classic
    // copy should win. They are still listed so that an install which has only
    // the re-release is at least found rather than silently missed.
    "Ultimate Doom/rerelease",
    "DOOM + DOOM II/rerelease",
    "Doom 2/rerelease",
  };
  size_t i;

  for (i = 0; i < sizeof subdirs / sizeof *subdirs; i++)
    AddDir("%s/steamapps/common/%s", lib, subdirs[i]);
}

static void BuildSteamDirs(void)
{
  char root[MAXPATHLN];
  const char *home;

  steamdirs_built = 1;

#if defined(_WIN32) && !defined(__CYGWIN__)
  {
    // Steam records where it was installed; do not assume Program Files, since
    // plenty of people move it to another drive.
    static const struct { HKEY hive; const char *key, *value; } regs[] =
    {
      { HKEY_CURRENT_USER,  "Software\\Valve\\Steam",              "SteamPath"   },
      { HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath" },
      { HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam",              "InstallPath" },
    };
    size_t i;

    for (i = 0; i < sizeof regs / sizeof *regs; i++)
      {
        HKEY hkey;
        DWORD type, len = sizeof root;

        if (RegOpenKeyExA(regs[i].hive, regs[i].key, 0, KEY_READ, &hkey)
            != ERROR_SUCCESS)
          continue;

        if (RegQueryValueExA(hkey, regs[i].value, NULL, &type,
                             (LPBYTE)root, &len) == ERROR_SUCCESS
            && type == REG_SZ)
          {
            root[sizeof root - 1] = 0;
            if (DirExists(root))
              {
                AddLibrary(root);
                AddLibrariesFromVdf(root, AddLibrary);
              }
          }
        RegCloseKey(hkey);
      }

    // last-ditch defaults
    {
      static const char *const guesses[] =
      {
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam",
      };
      for (i = 0; i < sizeof guesses / sizeof *guesses; i++)
        if (DirExists(guesses[i]))
          {
            AddLibrary(guesses[i]);
            AddLibrariesFromVdf(guesses[i], AddLibrary);
          }
    }
  }
#else
  if ((home = getenv("HOME")) && *home)
    {
      static const char *const relative[] =
      {
#ifdef __APPLE__
        "%s/Library/Application Support/Steam",
#else
        "%s/.steam/steam",
        "%s/.steam/root",
        "%s/.local/share/Steam",
        // Flatpak keeps its own copy of the home directory
        "%s/.var/app/com.valvesoftware.Steam/data/Steam",
        "%s/.var/app/com.valvesoftware.Steam/.local/share/Steam",
#endif
      };
      size_t i;

      for (i = 0; i < sizeof relative / sizeof *relative; i++)
        {
          snprintf(root, sizeof root, relative[i], home);
          if (DirExists(root))
            {
              AddLibrary(root);
              AddLibrariesFromVdf(root, AddLibrary);
            }
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// public interface
// ---------------------------------------------------------------------------

const char *D_SteamIWADDir(int n)
{
  if (!steamdirs_built)
    BuildSteamDirs();
  return (n >= 0 && n < numsteamdirs) ? steamdirs[n] : NULL;
}

int D_NumSteamIWADDirs(void)
{
  if (!steamdirs_built)
    BuildSteamDirs();
  return numsteamdirs;
}

// Try one filename in one directory, in three spellings: as given, all
// lowercase and all uppercase. This matters on Linux and macOS, where the
// lookup is a plain stat(): SMACK's standard names are lowercase, but Steam's
// classic packaging ships DOOM.WAD and DOOM2.WAD in capitals.
static int TryName(const char *dir, const char *name, char *out, size_t outlen)
{
  static const int cases[] = { 0, 1, 2 };
  size_t i, c;

  for (c = 0; c < sizeof cases / sizeof *cases; c++)
    {
      const char *slash = (*name == '/' || *name == '\\') ? "" : "/";

      snprintf(out, outlen, "%s%s%s", dir, slash, name);

      if (cases[c])
        {
          // only touch the filename, never the directory part
          char *base = strrchr(out, '/');
          char *bs   = strrchr(out, '\\');
          if (bs > base)
            base = bs;
          base = base ? base + 1 : out;
          for (i = 0; base[i]; i++)
            base[i] = (char)(cases[c] == 1 ? tolower((unsigned char)base[i])
                                           : toupper((unsigned char)base[i]));
        }

      if (FileExists(out))
        return 1;
    }

  return 0;
}

const char *D_SteamFindIWAD(const char *const *names, int nnames)
{
  static char found[MAXPATHLN];
  int d, i, ndirs;

  ndirs = D_NumSteamIWADDirs();

  // Directory order is priority order, so a name found in the re-release beats
  // the same name in the DOSBox copy. Within a directory the caller's name
  // order decides (doom2.wad before doom.wad, as in standard_iwads).
  for (d = 0; d < ndirs; d++)
    for (i = 0; i < nnames; i++)
      if (TryName(steamdirs[d], names[i], found, sizeof found))
        return found;

  return NULL;
}
