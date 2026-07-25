// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// DESCRIPTION:
//      SDL3 main program. Calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

#include "../doomdef.h"
#include "../m_argv.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../z_zone.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL.h>

static char errmsg_buf[2048];
static int  has_exited;

// signal handler -- kept as a sanity net for SIGSEGV/SIGILL/SIGFPE.
// in normal use we exit via I_Error or I_Quit.
static void signal_handler(int s)
{
  const char *msg;
  switch (s)
  {
    case SIGSEGV: msg = "Segmentation Violation"; break;
    case SIGINT:  msg = "Interrupted by User";    break;
    case SIGILL:  msg = "Illegal Instruction";    break;
    case SIGFPE:  msg = "Floating Point Exception"; break;
    case SIGTERM: msg = "Killed";                 break;
    case SIGABRT: msg = "Aborted";                break;
    default:      msg = "Terminated by signal";   break;
  }
  signal(s, SIG_IGN);

  // dump zone history for memory-related crashes
  if (s == SIGSEGV || s == SIGILL || s == SIGFPE)
    Z_DumpHistory((char *)msg);

  fprintf(stderr, "\n%s\n", msg);
  fflush(stderr);
  _exit(1);
}

int main(int argc, char **argv)
{
  myargc = argc;
  myargv = argv;

  has_exited = 0;
  errmsg_buf[0] = 0;

  // initialise SDL as early as possible; this lets the high-DPI hint
  // and the platform driver come up before we touch the video layer.
  if (!SDL_Init(0))
  {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Z_Init();

  // atexit shutdown handler. I_Quit defined in i_system.c
  atexit(I_Quit);

  signal(SIGSEGV, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGILL,  signal_handler);
  signal(SIGFPE,  signal_handler);
  signal(SIGINT,  signal_handler);
  signal(SIGABRT, signal_handler);

  D_DoomMain();

  // run atexit handlers and shut SDL down
  SDL_Quit();
  return 0;
}
