// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// UMAPINFO parsing -- see p_umapinfo.h.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "doomstat.h"
#include "c_io.h"
#include "d_main.h"    // usermsg
#include "w_wad.h"
#include "z_zone.h"
#include "p_info.h"
#include "p_umapinfo.h"

typedef struct
{
  char  mapname[9];
  char *levelname, *label, *levelpic;
  char *nextmap, *nextsecret;
  char *music, *skytexture;
  char *intertext, *interbackdrop;
  int   partime;          // -1 unset
  int   endgame;          // -1 unset
  int   nointermission;   // -1 unset
} umapentry_t;

static umapentry_t *umapinfo;
static int          numumapinfo;

// ---------------------------------------------------------------------------
// tokenizer
//
// UMAPINFO is a small brace / `key = value` language. Values are quoted
// strings, bare identifiers (true/false) or numbers, and a string value may be
// a comma-separated list continued across lines (intertext). Unrecognised keys
// are skipped rather than rejected: the format is meant to be
// forward-compatible, so a wad using a key from a later revision must still
// load here.
// ---------------------------------------------------------------------------

static char *tp, *tend;

static void SkipSpace(void)
{
  while (tp < tend)
    {
      if (*tp == '/' && tp + 1 < tend && tp[1] == '/')
        while (tp < tend && *tp != '\n') tp++;
      else if (*tp == '/' && tp + 1 < tend && tp[1] == '*')
        {
          tp += 2;
          while (tp + 1 < tend && !(*tp == '*' && tp[1] == '/')) tp++;
          tp += 2;
        }
      else if (isspace((unsigned char)*tp))
        tp++;
      else
        break;
    }
}

// Quotes are stripped, but whether it *was* quoted is reported, so `true` can
// be told from `"true"`.
static int NextToken(char *buf, size_t bufsz, int *wasquoted)
{
  size_t n = 0;

  if (wasquoted) *wasquoted = 0;
  SkipSpace();
  if (tp >= tend) return 0;

  if (*tp == '"')
    {
      if (wasquoted) *wasquoted = 1;
      tp++;
      while (tp < tend && *tp != '"')
        {
          if (n < bufsz - 1) buf[n++] = *tp;
          tp++;
        }
      if (tp < tend) tp++;
    }
  else if (*tp == '{' || *tp == '}' || *tp == '=' || *tp == ',')
    buf[n++] = *tp++;
  else
    while (tp < tend && !isspace((unsigned char)*tp) &&
           *tp != '{' && *tp != '}' && *tp != '=' && *tp != ',' && *tp != '"')
      {
        if (n < bufsz - 1) buf[n++] = *tp;
        tp++;
      }

  buf[n] = 0;
  return 1;
}

static int PeekToken(char *buf, size_t bufsz)
{
  char *save = tp;
  int r = NextToken(buf, bufsz, NULL);
  tp = save;
  return r;
}

static char *DupString(const char *s)
{
  char *p = Z_Malloc(strlen(s) + 1, PU_STATIC, 0);
  strcpy(p, s);
  return p;
}

// A value is a number, an identifier, or one or more quoted strings joined
// with newlines (UMAPINFO's multi-line text form).
static char *ReadValue(void)
{
  char tok[512], acc[4096];
  int quoted;

  acc[0] = 0;
  if (!NextToken(tok, sizeof tok, &quoted))
    return DupString("");

  strncpy(acc, tok, sizeof acc - 1);
  acc[sizeof acc - 1] = 0;

  if (quoted)
    for (;;)
      {
        char peek[8];
        if (!PeekToken(peek, sizeof peek) || peek[0] != ',')
          break;
        NextToken(peek, sizeof peek, NULL);
        if (!NextToken(tok, sizeof tok, &quoted))
          break;
        if (strlen(acc) + strlen(tok) + 2 < sizeof acc)
          {
            strcat(acc, "\n");
            strcat(acc, tok);
          }
      }

  return DupString(acc);
}

static umapentry_t *FindOrAddMap(const char *name)
{
  int i;

  for (i = 0; i < numumapinfo; i++)
    if (!strncasecmp(umapinfo[i].mapname, name, 8))
      return &umapinfo[i];

  umapinfo = realloc(umapinfo, (numumapinfo + 1) * sizeof *umapinfo);
  memset(&umapinfo[numumapinfo], 0, sizeof *umapinfo);
  strncpy(umapinfo[numumapinfo].mapname, name, 8);
  umapinfo[numumapinfo].partime = -1;
  umapinfo[numumapinfo].endgame = -1;
  umapinfo[numumapinfo].nointermission = -1;
  return &umapinfo[numumapinfo++];
}

static void ParseMapBlock(umapentry_t *e)
{
  char tok[512];

  if (!NextToken(tok, sizeof tok, NULL) || tok[0] != '{')
    return;

  for (;;)
    {
      char key[128];

      if (!NextToken(key, sizeof key, NULL) || key[0] == '}')
        break;

      if (!PeekToken(tok, sizeof tok) || tok[0] != '=')
        continue;                        // malformed -- skip
      NextToken(tok, sizeof tok, NULL);

      if      (!strcasecmp(key, "levelname"))      e->levelname     = ReadValue();
      else if (!strcasecmp(key, "label"))          e->label         = ReadValue();
      else if (!strcasecmp(key, "levelpic"))       e->levelpic      = ReadValue();
      else if (!strcasecmp(key, "next"))           e->nextmap       = ReadValue();
      else if (!strcasecmp(key, "nextsecret"))     e->nextsecret    = ReadValue();
      else if (!strcasecmp(key, "music"))          e->music         = ReadValue();
      else if (!strcasecmp(key, "skytexture"))     e->skytexture    = ReadValue();
      else if (!strcasecmp(key, "intertext"))      e->intertext     = ReadValue();
      else if (!strcasecmp(key, "interbackdrop"))  e->interbackdrop = ReadValue();
      else if (!strcasecmp(key, "par"))            e->partime = atoi(ReadValue());
      else if (!strcasecmp(key, "endgame"))        e->endgame = !strcasecmp(ReadValue(), "true");
      else if (!strcasecmp(key, "nointermission")) e->nointermission = !strcasecmp(ReadValue(), "true");
      else
        {
          // Unknown key (bossaction, endpic, author, ...). Consume its value,
          // and any comma-separated continuation, so the parser stays in step.
          ReadValue();
          while (PeekToken(tok, sizeof tok) && tok[0] == ',')
            { NextToken(tok, sizeof tok, NULL); ReadValue(); }
        }
    }
}

static void ParseLump(int lumpnum)
{
  char *lump = W_CacheLumpNum(lumpnum, PU_STATIC);
  char tok[512];

  tp   = lump;
  tend = lump + W_LumpLength(lumpnum);

  while (NextToken(tok, sizeof tok, NULL))
    if (!strcasecmp(tok, "map"))
      {
        char name[64];
        if (!NextToken(name, sizeof name, NULL))
          break;
        ParseMapBlock(FindOrAddMap(name));
      }

  Z_ChangeTag(lump, PU_CACHE);
}

void P_LoadUMapInfo(void)
{
  int i;

  numumapinfo = 0;
  free(umapinfo);
  umapinfo = NULL;

  // Every UMAPINFO in load order, so a later pwad wins: FindOrAddMap reuses the
  // entry and the later value overrides per key.
  for (i = 0; i < numlumps; i++)
    if (!strncasecmp(lumpinfo[i]->name, "UMAPINFO", 8))
      ParseLump(i);

  if (numumapinfo)
    usermsg("P_LoadUMapInfo: %d map%s from UMAPINFO",
            numumapinfo, numumapinfo == 1 ? "" : "s");
}

boolean P_ApplyUMapInfo(const char *mapname)
{
  int i;

  for (i = 0; i < numumapinfo; i++)
    {
      umapentry_t *e = &umapinfo[i];

      if (strncasecmp(e->mapname, mapname, 8))
        continue;

      // Only override what this entry actually specifies; anything else keeps
      // whatever P_LoadLevelInfo worked out.
      if (e->levelname)     info_levelname = e->levelname;
      if (e->levelpic)      info_levelpic  = e->levelpic;
      if (e->music)         info_music     = e->music;
      if (e->skytexture)    info_skyname   = e->skytexture;
      if (e->nextmap)       info_nextlevel = e->nextmap;
      if (e->intertext)     info_intertext = e->intertext;
      if (e->interbackdrop) info_backdrop  = e->interbackdrop;
      if (e->partime >= 0)  info_partime   = e->partime;

      // usermsg goes to the in-game console once the game is running, so
      // echo to stdout under -devparm where it can actually be read.
      usermsg("UMAPINFO: %s \"%s\"", e->mapname,
              info_levelname ? info_levelname : "");
      if (devparm)
        printf("UMAPINFO: %s \"%s\" next=%s par=%d\n", e->mapname,
               info_levelname ? info_levelname : "",
               e->nextmap ? e->nextmap : "-", info_partime);
      return true;
    }

  return false;
}
