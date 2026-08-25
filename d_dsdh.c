//-----------------------------------------------------------------------------
//
// DSDHacked: unlimited state and thing arrays.
//
// SMACK's DEHACKED parser (d_deh.c) was written against the original
// fixed-size `states[NUMSTATES]` / `mobjinfo[NUMMOBJTYPES]` globals. Its
// bounds checks on out-of-range Frame/Thing/[CODEPTR] indices are
// inconsistent -- some just print a warning and then go on to write past
// the end of the array anyway (deh_procFrame), and deh_procThing has no
// check at all -- so a DEHACKED patch naming a state or thing past the end
// of either table corrupts memory beyond it, today, on a stock build.
//
// This gives both tables room to grow, ported from dsda-doom/Woof's
// DSDHacked feature (originally by Xaser Acheron/Kraflab, later folded into
// mainline DEHACKED as "DEHEXTRA"): DSDH_Init() replaces the two globals
// with malloc'd copies of the compile-time tables (still in info.c, renamed
// original_states/original_mobjinfo); DSDH_StateTranslate/DSDH_ThingTranslate
// map a DEHACKED index to a states[]/mobjinfo[] slot, growing the table and
// remembering the mapping the first time an out-of-range index is seen.
//
// One deliberate simplification versus Woof: Woof keeps the index mapping in
// a hash table (src/m_hashmap.c), because DSDHacked patches can in principle
// reference arbitrarily large frame/thing numbers and it wants O(1) lookups.
// SMACK has no hash table of its own and this only runs while parsing
// DEHACKED patches at startup (a few times, not per-tic), so a linear-scan
// array is simplest and plenty fast for the handful of extended entries any
// real patch defines.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomdef.h"    // pulls in z_zone.h, which routes malloc/realloc
                         // through the zone allocator (PU_STATIC)
#include "info.h"
#include "d_dsdh.h"

// info.c: the compile-time tables this file copies from and grows beyond
extern state_t    original_states[NUMSTATES];
extern mobjinfo_t original_mobjinfo[NUMMOBJTYPES];

state_t *states;
int      num_states;

mobjinfo_t *mobjinfo;
int         num_mobj_types;

// ----------------------------------------------------------------------
// Index-translation tables: linear-scan (key, index) pairs. See file
// comment for why this isn't a hash table.
// ----------------------------------------------------------------------

typedef struct
{
  int key;      // the DEHACKED-side index that triggered the extension
  int index;    // the states[]/mobjinfo[] slot it was assigned
} dsdh_xlat_t;

static dsdh_xlat_t *state_xlat;
static int          state_xlat_num, state_xlat_cap;

static dsdh_xlat_t *thing_xlat;
static int          thing_xlat_num, thing_xlat_cap;

static int XlatLookup(dsdh_xlat_t *tab, int num, int key)
{
  int i;

  for (i = 0; i < num; i++)
    if (tab[i].key == key)
      return tab[i].index;

  return -1;
}

static void XlatAdd(dsdh_xlat_t **tab, int *num, int *cap, int key, int index)
{
  if (*num == *cap)
    {
      *cap = *cap ? *cap * 2 : 32;
      *tab = realloc(*tab, *cap * sizeof **tab);
    }

  (*tab)[*num].key   = key;
  (*tab)[*num].index = index;
  ++*num;
}

void DSDH_Init(void)
{
  num_states = NUMSTATES;
  states = malloc(NUMSTATES * sizeof *states);
  memcpy(states, original_states, NUMSTATES * sizeof *states);

  num_mobj_types = NUMMOBJTYPES;
  mobjinfo = malloc(NUMMOBJTYPES * sizeof *mobjinfo);
  memcpy(mobjinfo, original_mobjinfo, NUMMOBJTYPES * sizeof *mobjinfo);
}

int DSDH_StateTranslate(int frame_number)
{
  int index, new_index;
  state_t blank = {SPR_TNT1, 0, -1, NULL, 0, 0, 0, {0}};

  if (frame_number < 0)
    return S_NULL;      // malformed patch; not worth growing the table for

  if (frame_number < NUMSTATES)
    return frame_number;

  index = XlatLookup(state_xlat, state_xlat_num, frame_number);
  if (index >= 0)
    return index;

  new_index = num_states;
  blank.nextstate = new_index;   // self-loop: an inert placeholder frame,
                                  // same as an unpatched Frame block leaves it
  states = realloc(states, (num_states + 1) * sizeof *states);
  states[new_index] = blank;
  ++num_states;

  XlatAdd(&state_xlat, &state_xlat_num, &state_xlat_cap, frame_number, new_index);
  return new_index;
}

int DSDH_ThingTranslate(int thing_number)
{
  int index, new_index;
  mobjinfo_t blank;

  if (thing_number < 0)
    return 0;

  if (thing_number < NUMMOBJTYPES)
    return thing_number;

  index = XlatLookup(thing_xlat, thing_xlat_num, thing_number);
  if (index >= 0)
    return index;

  new_index = num_mobj_types;
  memset(&blank, 0, sizeof blank);
  mobjinfo = realloc(mobjinfo, (num_mobj_types + 1) * sizeof *mobjinfo);
  mobjinfo[new_index] = blank;
  ++num_mobj_types;

  XlatAdd(&thing_xlat, &thing_xlat_num, &thing_xlat_cap, thing_number, new_index);
  return new_index;
}

//-----------------------------------------------------------------------------
