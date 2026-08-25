//-----------------------------------------------------------------------------
//
// DSDHacked: unlimited state and thing arrays.
//
// See d_dsdh.c for the rationale.
//
//-----------------------------------------------------------------------------

#ifndef __D_DSDH__
#define __D_DSDH__

// Replaces states[]/mobjinfo[] with malloc'd copies of the compile-time
// tables. Call once, before any DEHACKED file is processed.
void DSDH_Init(void);

// frame_number/thing_number are DEHACKED's own numbering (thing_number is
// already the zero-based mobjinfo index, i.e. the caller has done the usual
// "Thing N" -> N-1 conversion). Returns the states[]/mobjinfo[] index to use:
// itself, if within the original table; otherwise a new slot, growing the
// table and remembering the mapping so repeat references resolve the same way.
int DSDH_StateTranslate(int frame_number);
int DSDH_ThingTranslate(int thing_number);

#endif

//-----------------------------------------------------------------------------
