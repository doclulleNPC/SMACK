// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// DESCRIPTION:
//      System interface: timer, error, quit, abort check.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include "../c_runcmd.h"
#include "../i_system.h"
#include "../i_sound.h"
#include "../i_video.h"
#include "../doomstat.h"
#include "../m_misc.h"
#include "../g_game.h"
#include "../w_wad.h"
#include "../v_video.h"
#include "../m_argv.h"
#include "../d_event.h"
#include "../version.h"

// ---- timing ---------------------------------------------------------------

static unsigned long basetime;

static int I_GetTime_RealTime_internal(void)
{
  struct timeval tv;
  unsigned long thistimereply;

  gettimeofday(&tv, NULL);
  thistimereply = (unsigned long)(tv.tv_sec) * TICRATE
                + (unsigned long)(tv.tv_usec) * TICRATE / 1000000UL;

  if (!basetime)
  {
    basetime = thistimereply;
    thistimereply = 0;
  }
  else
    thistimereply -= basetime;

  return (int)thistimereply;
}

int I_GetTime_RealTime(void)
{
  return I_GetTime_RealTime_internal();
}

static long long  I_GetTime_Scale = 1 << 16;
static int        I_GetTime_FastDemo_state;

static int I_GetTime_FastDemo(void)
{
  return I_GetTime_FastDemo_state++;
}

static int I_GetTime_Scaled(void)
{
  // unused on Linux: I_GetTime is always real-time here.
  return I_GetTime_RealTime();
}

static int I_GetTime_Error(void)
{
  I_Error("Error: GetTime() used before initialization");
  return 0;
}

int (*I_GetTime)(void) = I_GetTime_Error;
int  realtic_clock_rate = 100;

void I_SetTime(int newtime)
{
  // not used on Linux (real-time clock); provided for API compatibility.
  (void)newtime;
}

// ---- input bookkeeping ----------------------------------------------------

int mousepresent     = 1;
int joystickpresent  = 0;
int keyboard_installed = 1;
int leds_always_off  = 0;
extern int autorun;

ticcmd_t *I_BaseTiccmd(void)
{
  static ticcmd_t emptycmd;
  return &emptycmd;
}

void I_WaitVBL(int count)
{
  // SDL3 does its own frame pacing; this is a no-op now.
  (void)count;
}

void I_StartFrame(void)
{
}

void I_ResetLEDs(void)
{
}

// ---- shutdown / error ------------------------------------------------------

static char errmsg[2048];
static int  has_exited;

void I_Shutdown(void)
{
  // platform-specific shutdown: SDL_Quit happens in main() atexit.
}

void I_Quit(void)
{
  has_exited = 1;

  if (demorecording)
    G_CheckDemoStatus();

  if (*errmsg)
    puts(errmsg);
  else
    I_EndDoom();

  M_SaveDefaults();
}

void I_Error(const char *error, ...)
{
  va_list argptr;

  if (!*errmsg)  // only the first error message
  {
    va_start(argptr, error);
    vsnprintf(errmsg, sizeof(errmsg), error, argptr);
    va_end(argptr);
  }

  if (!has_exited)
  {
    has_exited = 1;
    // give the user something to read in the terminal
    fprintf(stderr, "\nI_Error: %s\n", errmsg);
    fflush(stderr);
    exit(1);  // triggers atexit(I_Quit)
  }
}

void I_EndDoom(void)
{
  // ENDOOM lump not used on Linux -- output nothing.
}

// ---- abort handling --------------------------------------------------------

int I_CheckAbort(void)
{
  // check event queue for ESC key
  event_t *ev;
  I_StartTic();
  for ( ; eventtail != eventhead; eventtail = (++eventtail) & (MAXEVENTS-1))
  {
    ev = &events[eventtail];
    if (ev->type == ev_keydown && ev->data1 == key_escape)
      return 1;
  }
  return 0;
}

// ---- init ------------------------------------------------------------------

extern void I_Sound_AddCommands(void);
extern void I_Video_AddCommands(void);

void I_AddCommands(void)
{
  I_Video_AddCommands();
  I_Sound_AddCommands();
}

void I_Init(void)
{
  int clock_rate = realtic_clock_rate;
  int p;

  if ((p = M_CheckParm("-speed")) && p < myargc-1 &&
      (p = atoi(myargv[p+1])) >= 10 && p <= 1000)
    clock_rate = p;

  if (fastdemo)
    I_GetTime = I_GetTime_FastDemo;
  else if (clock_rate != 100)
  {
    I_GetTime_Scale = ((long long)clock_rate << 16) / 100;
    I_GetTime = I_GetTime_Scaled;
  }
  else
    I_GetTime = I_GetTime_RealTime;

  // I_Quit saves the config (M_SaveDefaults) and finalizes demos on exit.
  // It must be the atexit handler so the `quit` command's exit(0), and any
  // exit(1) from I_Error, both save settings. (Was atexit(I_Shutdown), an empty
  // stub, so settings were never written -- despite the comment above claiming
  // exit() "triggers atexit(I_Quit)".) Crashes still _exit() past atexit.
  atexit(I_Quit);
  atexit(I_Shutdown);

  // avoid sound init if both flags are set
  {
    extern boolean nomusicparm, nosfxparm;
    if (!(nomusicparm && nosfxparm))
      I_InitSound();
  }
}
