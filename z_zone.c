// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// DESCRIPTION:
//      Zone Memory Allocation. Neat.
//
// 2026-07-23: Rewritten in the style of Woof (Fabian Greffrath's Doom port,
// src/z_zone.c) -- a thin per-block malloc wrapper -- to fix the 64-bit
// Linux segfault that the original MBF/SMMU zone allocator hit because
// it relies on a hard-coded HEADER_SIZE=32 which no longer matches the
// natural padding of memblock_t on LP64 targets.
//
// The public API in z_zone.h is unchanged: file/line are still appended
// to every public function so the INSTRUMENTED bookkeeping in other
// files keeps working, but the actual allocation strategy is now a
// one-malloc-per-block scheme with a per-tag linked list and a ZONEID
// magic number in the header for double-free detection.
//
// Woof is GPLv2+, SMMU is GPLv2 (per its file headers). The
// translation below adapts Woof's allocator to SMMU's 5-arg signatures
// (size, tag, user, file, line) and preserves the few SMMU-specific
// entry points (Z_DumpHistory, Z_ZoneHistory, Z_ReInit, Z_Init).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "z_zone.h"
#include "doomstat.h"

// SMMU's z_zone.h defines convenience macros (Z_Malloc, Z_Free, ...)
// that append __FILE__/__LINE__ to every call, and also aliases the
// C library malloc/free/realloc/... to call into the zone. Inside
// this implementation we have to call the underlying (Z_Malloc) etc.
// with already-supplied file/line arguments, and we want free() to
// mean the libc free() when we free a memblock_t. So we unbind
// every macro before defining the functions to stop the preprocessor
// from double-adding the file/line pair. After this block the symbols
// refer to the actual (parenthesised) function names.
#undef Z_Free
#undef Z_FreeTags
#undef Z_ChangeTag
#undef Z_Malloc
#undef Z_Strdup
#undef Z_Calloc
#undef Z_Realloc
#undef Z_CheckHeap
#undef malloc
#undef free
#undef realloc
#undef calloc
#undef strdup

// Minimum chunk size at which blocks are allocated. On 64-bit this is
// 8 bytes (pointer size); on 32-bit it was 4. Keeping it pointer-sized
// avoids misaligned headers.
#define CHUNK_SIZE sizeof(void *)

// Magic number written into every block header. Used to detect
// double-frees and wild pointer frees.
#define ZONEID  0x931d4a11

typedef struct memblock {
  struct memblock *next, *prev;
  size_t size;
  void **user;
  unsigned id;
  int tag;                       // SMMU keeps tag as plain int
} memblock_t;

// Woof's trick: HEADER_SIZE is the size of the struct rounded up to the
// chunk alignment. Computed once at program start so the runtime check
// and the allocator can never disagree.
static size_t HEADER_SIZE;

// One linked list head per tag. PU_FREE is unused (we don't keep a
// free list at all -- the C library does that for us).
static memblock_t *blockbytag[PU_MAX];

// SMMU's old "zone" global is referenced by Z_FreeTags callers via
// extern, but the new allocator doesn't need it. Keep a NULL stub
// to avoid touching the rest of the engine.
memblock_t *zone = NULL;

// -- SMMU-specific compatibility shims ------------------------------------
// These functions existed in the old zone allocator and are referenced
// from doomdef.h / i_main.c. Woof's allocator doesn't need them, so we
// keep them as harmless no-ops that match the SMMU declarations.

static int has_exited = 0;

void Z_ReInit(void)
{
  // No-op in the Woof-style allocator. The old code re-initialised the
  // zone after a potential overflow; the new one lets malloc() handle
  // growth, so there is nothing to re-init.
}

void Z_Close(void)
{
  has_exited = 1;
  for (int t = 0; t < PU_MAX; t++)
    {
      memblock_t *block = blockbytag[t];
      if (!block)
        continue;
      memblock_t *end = block->prev;
      while (1)
        {
          memblock_t *next = block->next;
          free(block);
          if (block == end)
            break;
          block = next;
        }
      blockbytag[t] = NULL;
    }
}

void Z_DumpHistory(char *buf)
{
  // Old code dumped a malloc/free history. We don't keep one. A
  // placeholder lets i_main.c's signal handler compile.
  (void)buf;
}

void Z_ZoneHistory(char *buf)
{
  // Same -- a no-op stub for i_main.c.
  (void)buf;
}

void Z_Init(void)
{
  // Compute HEADER_SIZE exactly once. The result is (rounded-up
  // sizeof(memblock_t)) which is what every allocation in this file
  // uses to find the user pointer from the raw malloc() result.
  HEADER_SIZE = (sizeof(memblock_t) + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);
  memset(blockbytag, 0, sizeof(blockbytag));
  atexit(Z_Close);
}

// -- Public API ------------------------------------------------------------
//
// Every public function takes the SMMU file/line pair so existing
// call-sites compile unchanged. We ignore file/line because the Woof
// allocator doesn't keep per-block history; the SMMU macros in
// z_zone.h still expand them so the callers don't need edits.

void *(Z_Malloc)(size_t size, int tag, void **user, const char *file, int line)
{
  memblock_t *block;

  (void)file; (void)line;

  if (tag == PU_CACHE && !user)
    I_Error("An owner is required for purgable blocks");

  if (!size)
    return user ? *user = NULL : NULL;

  while (!(block = (memblock_t *) malloc(size + HEADER_SIZE)))
    {
      if (!blockbytag[PU_CACHE])
        I_Error("Failure trying to allocate %zu bytes", size);
      // Direct call to the function -- bypasses the Z_FreeTags(lo,hi)
      // macro in z_zone.h which would append file/line again and
      // turn this 4-arg call into 6 args.
      (Z_FreeTags)(PU_CACHE, PU_CACHE, file, line);
    }

  // Insert into the per-tag circular list.
  if (!blockbytag[tag])
    {
      blockbytag[tag] = block;
      block->next = block->prev = block;
    }
  else
    {
      blockbytag[tag]->prev->next = block;
      block->prev = blockbytag[tag]->prev;
      block->next = blockbytag[tag];
      blockbytag[tag]->prev = block;
    }

  block->size = size;
  block->id   = ZONEID;
  block->tag  = tag;
  block->user = user;

  // Return the user-data pointer, which lives HEADER_SIZE bytes past
  // the raw block start.
  block = (memblock_t *)((char *) block + HEADER_SIZE);
  if (user)
    *user = block;

  return block;
}

void (Z_Free)(void *p, const char *file, int line)
{
  memblock_t *block;

  (void)file; (void)line;

  if (!p)
    return;

  block = (memblock_t *)((char *) p - HEADER_SIZE);

  if (block->id != ZONEID)
    I_Error("Z_Free: freed a pointer without ZONEID");

  block->id = 0;            // prevent double-free

  if (block->user)          // null the user pointer if there is one
    *block->user = NULL;

  // Unlink from the per-tag list.
  if (block == block->next)
    blockbytag[block->tag] = NULL;
  else if (blockbytag[block->tag] == block)
    blockbytag[block->tag] = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  free(block);
}

void (Z_FreeTags)(int lowtag, int hightag, const char *file, int line)
{
  (void)file; (void)line;

  if (lowtag < 0 || hightag >= PU_MAX)
    I_Error("Z_FreeTags: invalid tag range %d..%d", lowtag, hightag);

  for (int tag = lowtag; tag <= hightag; tag++)
    {
      memblock_t *block = blockbytag[tag];
      if (!block)
        continue;
      memblock_t *end = block->prev;
      while (1)
        {
          memblock_t *next = block->next;
          (Z_Free)((char *) block + HEADER_SIZE, file, line);
          if (block == end)
            break;
          block = next;
        }
    }
}

void (Z_ChangeTag)(void *p, int tag, const char *file, int line)
{
  memblock_t *block;

  (void)file; (void)line;

  if (!p)
    return;

  block = (memblock_t *)((char *) p - HEADER_SIZE);

  if (block->id != ZONEID)
    I_Error("Z_ChangeTag: pointer without ZONEID");

  if (tag == block->tag)
    return;

  if (tag == PU_CACHE && !block->user)
    I_Error("Z_ChangeTag: an owner is required for purgable blocks");

  // Unlink from old list.
  if (block == block->next)
    blockbytag[block->tag] = NULL;
  else if (blockbytag[block->tag] == block)
    blockbytag[block->tag] = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  // Insert into new list.
  if (!blockbytag[tag])
    {
      blockbytag[tag] = block;
      block->next = block->prev = block;
    }
  else
    {
      blockbytag[tag]->prev->next = block;
      block->prev = blockbytag[tag]->prev;
      block->next = blockbytag[tag];
      blockbytag[tag]->prev = block;
    }

  block->tag = tag;
}

void *(Z_Calloc)(size_t n1, size_t n2, int tag, void **user,
                 const char *file, int line)
{
  return (n1 *= n2)
       ? memset((Z_Malloc)(n1, tag, user, file, line), 0, n1)
       : NULL;
}

void *(Z_Realloc)(void *p, size_t n, int tag, void **user,
                  const char *file, int line)
{
  void *new_p = (Z_Malloc)(n, tag, user, file, line);
  if (p)
    {
      memblock_t *block = (memblock_t *)((char *) p - HEADER_SIZE);
      memcpy(new_p, p, n <= block->size ? n : block->size);
      (Z_Free)(p, file, line);
      if (user)            // Z_Free nullified *user, restore
        *user = new_p;
    }
  return new_p;
}

char *(Z_Strdup)(const char *s, int tag, void **user,
                 const char *file, int line)
{
  size_t n = strlen(s) + 1;
  char *r = (char *)(Z_Malloc)(n, tag, user, file, line);
  memcpy(r, s, n);
  return r;
}

void (Z_CheckHeap)(const char *file, int line)
{
  // Woof-style allocator delegates to malloc(); the per-block ZONEID
  // already catches the misuse this function was historically used to
  // detect. Kept as a no-op for source compatibility.
  (void)file; (void)line;
}

// Print-stats stub for the INSTRUMENTED build path that asks for it.
#ifdef INSTRUMENTED
int printstats = 0;
char *(Z_PrintStats)(void) { return ""; }
#endif

//-----------------------------------------------------------------------------
//
// $Log$
//
// 2026-07-23 rewrite: replace 1999 zone allocator with Woof-style
// per-block malloc() wrapper to fix 64-bit struct padding bug.
//
