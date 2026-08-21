// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// DESCRIPTION:
//      SDL3 video driver for SMMU. 8-bit palettised Doom framebuffer
//      is converted to an RGBA8888 texture and stretched to fit the
//      window with nearest-neighbour sampling so the pixel art stays
//      sharp.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../c_io.h"
#include "../c_runcmd.h"
#include "../doomstat.h"
#include "../v_video.h"
#include "../d_main.h"
#include "../d_event.h"
#include "../g_game.h"
#include "../m_bbox.h"
#include "../st_stuff.h"
#include "../m_argv.h"
#include "../w_wad.h"
#include "../r_draw.h"
#include "../am_map.h"
#include "../mn_engin.h"
#include "../wi_stuff.h"
#include "../i_video.h"
#include "../version.h"

#include <SDL3/SDL.h>

static boolean initialised = false;
static boolean in_graphics_mode = false;

// ---- video mode table -----------------------------------------------------

videomode_t videomodes[] =
{
  { "320x200" },
  { "640x400 (hires)" },
  { NULL },
};

// ---- palette / gamma ------------------------------------------------------

extern byte gammatable[5][256];
extern int  usegamma;

static SDL_Color palette_rgb[256];

// ---- framebuffer staging --------------------------------------------------

static SDL_Window   *window      = NULL;
static SDL_Renderer *renderer    = NULL;
static SDL_Texture  *texture     = NULL;
static SDL_Surface  *rgba_surface = NULL;
static Uint32       *pixel_buffer = NULL;
static int           pitch_pixels = 0;

// Persisted window geometry (cvars v_width / v_height / v_fullscreen, saved in
// smack.cfg). 0x0 means "never set" -- a fresh config falls back to the scale
// rules below. Command-line options still win over the saved values, so
// -geom/-2/-3/-4 remain a one-shot override rather than something that
// silently rewrites your config.
int                  v_width;
int                  v_height;
int                  v_fullscreen;

static int           win_w = 1280;
static int           win_h = 800;
static int           scale = 1;     // window magnification applied on top of
                                   // the render framebuffer. SMACK_SCALE=N sets
                                   // it (default 1). With hires the framebuffer
                                   // is already 640x400, so scale=1 -> 640x400.
static int           hires_flag = 0;
// Render framebuffer size = SCREEN{WIDTH,HEIGHT} << hires. Not initialised
// from those here: they are runtime variables now (doomdef.h), so this is
// not a constant expression. I_CreateWindowAndRenderer sets both before
// anything reads them.
static int           fb_w;
static int           fb_h;

static SDL_PixelFormat    pixel_fmt;
static const SDL_PixelFormatDetails *pixel_details = NULL;

// ---- input ----------------------------------------------------------------

extern int usemouse;

// Whether the pointer is currently captured (relative mode + hidden cursor).
// We only grab during active gameplay; menus/console/pause release it so the
// desktop cursor comes back and can click menu items / other windows.
static boolean mouse_grabbed = false;

static boolean I_WantGrab(void)
{
  extern boolean menuactive;   // doomstat.h
  return usemouse && in_graphics_mode &&
         !menuactive && !consoleactive && !paused;
}

// Sync SDL's relative-mouse state to whether we want the pointer grabbed.
// Called every tic; cheap no-op when nothing changed.
static void I_UpdateGrab(void)
{
  boolean want = I_WantGrab();
  if (want == mouse_grabbed || !window)
    return;
  SDL_SetWindowRelativeMouseMode(window, want);
  if (!want)
    // drop any captured motion so re-grabbing doesn't lurch the view
    SDL_GetRelativeMouseState(NULL, NULL);
  mouse_grabbed = want;
}

static int sdl_keycode_to_doom(SDL_Keycode k)
{
  // printable ASCII -- send through
  if (k >= SDLK_SPACE && k <= SDLK_Z)
    return (int)k;

  switch (k)
  {
    case SDLK_ESCAPE:    return KEYD_ESCAPE;
    case SDLK_RETURN:    return KEYD_ENTER;
    case SDLK_TAB:       return KEYD_TAB;
    case SDLK_BACKSPACE: return KEYD_BACKSPACE;
    case SDLK_PAUSE:     return KEYD_PAUSE;
    case SDLK_EQUALS:    return KEYD_EQUALS;
    case SDLK_MINUS:     return KEYD_MINUS;

    case SDLK_F1:  return KEYD_F1;
    case SDLK_F2:  return KEYD_F2;
    case SDLK_F3:  return KEYD_F3;
    case SDLK_F4:  return KEYD_F4;
    case SDLK_F5:  return KEYD_F5;
    case SDLK_F6:  return KEYD_F6;
    case SDLK_F7:  return KEYD_F7;
    case SDLK_F8:  return KEYD_F8;
    case SDLK_F9:  return KEYD_F9;
    case SDLK_F10: return KEYD_F10;
    case SDLK_F11: return KEYD_F11;
    case SDLK_F12: return KEYD_F12;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return KEYD_RSHIFT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:  return KEYD_RCTRL;
    case SDLK_LALT:
    case SDLK_RALT:   return KEYD_RALT;

    case SDLK_UP:    return KEYD_UPARROW;
    case SDLK_DOWN:  return KEYD_DOWNARROW;
    case SDLK_LEFT:  return KEYD_LEFTARROW;
    case SDLK_RIGHT: return KEYD_RIGHTARROW;

    case SDLK_INSERT:    return KEYD_INSERT;
    case SDLK_HOME:      return KEYD_HOME;
    case SDLK_END:       return KEYD_END;
    case SDLK_PAGEUP:    return KEYD_PAGEUP;
    case SDLK_PAGEDOWN:  return KEYD_PAGEDOWN;
    case SDLK_DELETE:    return KEYD_DEL;
    case SDLK_CAPSLOCK:  return KEYD_CAPSLOCK;
    case SDLK_SCROLLLOCK: return KEYD_SCROLLLOCK;
    case SDLK_NUMLOCKCLEAR: return KEYD_NUMLOCK;

    default: return 0;
  }
}

static void post_key_event(SDL_Keycode k, boolean down)
{
  int code = sdl_keycode_to_doom(k);
  if (!code) return;
  event_t ev;
  ev.type  = down ? ev_keydown : ev_keyup;
  ev.data1 = code;
  ev.data2 = ev.data3 = 0;
  D_PostEvent(&ev);
}

// Mouse buttons are *state*, not edges: G_Responder assigns
// mousebuttons[n] = ev->data1 & bit on every ev_mouse, so each event must
// carry the full current mask. Two bugs used to break holding a button:
// motion events hardcoded data1 = 0, which released every mouse button as
// soon as you moved (so fire never repeated), and post_mouse_button ignored
// its `down` argument, so a release looked identical to a press.
static int mouse_button_state;

static void post_mouse_motion(int dx, int dy)
{
  event_t ev;
  ev.type  = ev_mouse;
  ev.data1 = mouse_button_state;
  ev.data2 = dx << 2;
  ev.data3 = -(dy << 2);
  D_PostEvent(&ev);
}

static void post_mouse_button(int sdl_button, boolean down)
{
  int mask = 0;
  event_t ev;

  if (sdl_button == SDL_BUTTON_LEFT)   mask |= 1;
  if (sdl_button == SDL_BUTTON_RIGHT)  mask |= 2;
  if (sdl_button == SDL_BUTTON_MIDDLE) mask |= 4;

  if (down)
    mouse_button_state |= mask;
  else
    mouse_button_state &= ~mask;

  ev.type  = ev_mouse;
  ev.data1 = mouse_button_state;
  ev.data2 = ev.data3 = 0;
  D_PostEvent(&ev);
}

// ---- gamepad --------------------------------------------------------------
//
// The engine already had the whole joystick path -- joyxmove/joyymove/
// joybuttons, the ev_joystick case in G_Responder, the joyb_* bindings and the
// "enable joystick" menu toggle -- but nothing ever opened a device or posted
// an ev_joystick, so the option did nothing. This is that missing half.
//
// G_BuildTiccmd only ever tests joyxmove/joyymove for sign, so the stick is
// reported digitally (-1/0/+1) outside a deadzone, like the DOS joystick did.

void I_SetFullscreen(void);  // defined below, next to the video cvars

extern int usejoystick;      // m_misc.c -- the "enable joystick" option
extern int joystickpresent;  // linux/i_system.c

static SDL_Gamepad   *gamepad;
static SDL_JoystickID gamepad_id;

#define JOY_DEADZONE 12000   // of 32767

static void I_OpenGamepad(SDL_JoystickID which)
{
  if (gamepad)               // one is plenty
    return;

  if ((gamepad = SDL_OpenGamepad(which)))
    {
      const char *name = SDL_GetGamepadName(gamepad);
      gamepad_id = which;
      joystickpresent = 1;
      printf("I_InitJoystick: %s\n", name ? name : "gamepad");
    }
}

static void I_CloseGamepad(void)
{
  if (gamepad)
    {
      SDL_CloseGamepad(gamepad);
      gamepad = NULL;
      joystickpresent = 0;
    }
}

void I_InitJoystick(void)
{
  int i, count = 0;
  SDL_JoystickID *ids;

  if (M_CheckParm("-nojoy"))
    return;

  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    {
      printf("I_InitJoystick: no gamepad subsystem (%s)\n", SDL_GetError());
      return;
    }

  // devices already plugged in when we start; hotplug arrives as an event
  if ((ids = SDL_GetGamepads(&count)))
    {
      for (i = 0; i < count && !gamepad; i++)
        I_OpenGamepad(ids[i]);
      SDL_free(ids);
    }
}

// Sampled once per tic and posted as a single ev_joystick, because
// G_Responder treats the event as complete state rather than as a change.
static void I_PollGamepad(void)
{
  event_t ev;
  int mask = 0, x = 0, y = 0;

  if (!gamepad)
    return;

  if (usejoystick)
    {
      Sint16 ax = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
      Sint16 ay = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);

      if (ax < -JOY_DEADZONE || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
        x = -1;
      else if (ax > JOY_DEADZONE || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
        x = 1;

      // negative y is forward, which is also the way SDL reports "stick up"
      if (ay < -JOY_DEADZONE || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
        y = -1;
      else if (ay > JOY_DEADZONE || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
        y = 1;

      // Bits 0..3 are joystick buttons 0..3, which is what the joyb_*
      // defaults bind to: fire, strafe, speed, use.
      if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) ||
          SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > JOY_DEADZONE)
        mask |= 1;
      if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST))
        mask |= 2;
      if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
        mask |= 4;
      if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST) ||
          SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
        mask |= 8;
    }
  // when the option is off we still post, so buttons held at the moment it was
  // switched off do not stay stuck down

  ev.type  = ev_joystick;
  ev.data1 = mask;
  ev.data2 = x;
  ev.data3 = y;
  D_PostEvent(&ev);
}

// ---- I_StartTic / I_GetEvent ---------------------------------------------

void I_GetEvent(void);

void I_StartTic(void)
{
  I_GetEvent();
  I_PollGamepad();
  I_UpdateGrab();
}

void I_GetEvent(void)
{
  SDL_Event ev;
  while (SDL_PollEvent(&ev))
  {
    switch (ev.type)
    {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        // Window close button / WM quit -> clean exit. exit(0) runs the
        // atexit handlers (I_Quit -> M_SaveDefaults), so settings are saved.
        // (Previously this only posted ESC, which opened the menu and never
        // quit or saved.) SDL3 sends CLOSE_REQUESTED for the window's X button
        // and QUIT when the last window closes -- handle both.
        exit(0);
        break;

      case SDL_EVENT_KEY_DOWN:
        // Alt+Enter toggles fullscreen and is swallowed, so it never reaches
        // the game as a "use" press.
        if (ev.key.key == SDLK_RETURN && (ev.key.mod & SDL_KMOD_ALT))
        {
          v_fullscreen = !v_fullscreen;
          I_SetFullscreen();
          break;
        }
        post_key_event(ev.key.key, true);
        break;

      case SDL_EVENT_KEY_UP:
        post_key_event(ev.key.key, false);
        break;

      case SDL_EVENT_MOUSE_MOTION:
        // only feed motion to the game while grabbed; otherwise the free
        // desktop cursor would keep turning the player in menus
        if (mouse_grabbed)
          post_mouse_motion(ev.motion.xrel, ev.motion.yrel);
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        post_mouse_button(ev.button.button, true);
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        post_mouse_button(ev.button.button, false);
        break;

      case SDL_EVENT_GAMEPAD_ADDED:
        I_OpenGamepad(ev.gdevice.which);
        break;

      case SDL_EVENT_GAMEPAD_REMOVED:
        if (gamepad && ev.gdevice.which == gamepad_id)
          I_CloseGamepad();
        break;

      case SDL_EVENT_WINDOW_RESIZED:
        win_w = ev.window.data1;
        win_h = ev.window.data2;
        // Remember it for next launch -- but not while fullscreen, or we would
        // save the monitor size as the windowed size and come back to a window
        // the size of the screen.
        if (!v_fullscreen)
        {
          v_width  = win_w;
          v_height = win_h;
        }
        break;

      default:
        break;
    }
  }
}

// ---- scancode helpers (killough 5/98) -------------------------------------

int I_DoomCode2ScanCode(int c) { return c; }
int I_ScanCode2DoomCode(int c) { return c; }

// ---- graphics frame lifecycle --------------------------------------------

extern int leds_always_off;  // defined in linux/i_system.c
int use_vsync = 0;
int disk_icon;
int vesamode;
int page_flip;
int hires = 1;        // global for SMMU's render code. Always 1 (640x400):
                      // lowres support has been removed. Kept as a variable
                      // because the whole renderer keys off SCREENWIDTH<<hires.

void I_UpdateNoBlit(void)
{
}

void I_ReadScreen(byte *scr)
{
  int size = (SCREENWIDTH << hires) * (SCREENHEIGHT << hires);
  memcpy(scr, screens[0], size);
}

void I_SetPalette(byte *pal)
{
  if (in_textmode || !in_graphics_mode)
    return;
  // gammatable holds full 8-bit (0..255) values. The original DOS code did
  // `>> 2` to fit the VGA DAC's 6-bit range; SDL wants full 8-bit RGB, so use
  // the value directly -- shifting made the whole image 1/4 brightness (dark).
  for (int i = 0; i < 256; i++)
  {
    palette_rgb[i].r = gammatable[usegamma][pal[3*i+0]];
    palette_rgb[i].g = gammatable[usegamma][pal[3*i+1]];
    palette_rgb[i].b = gammatable[usegamma][pal[3*i+2]];
    palette_rgb[i].a = 255;
  }
}

static void I_PaletteToRGBA(byte *src, int w, int h)
{
  for (int y = 0; y < h; y++)
  {
    Uint32 *dst = pixel_buffer + y * pitch_pixels;
    byte   *s   = src + y * w;
    for (int x = 0; x < w; x++)
    {
      byte idx = s[x];
      dst[x] = SDL_MapRGBA(pixel_details, NULL,
                           palette_rgb[idx].r,
                           palette_rgb[idx].g,
                           palette_rgb[idx].b,
                           255);
    }
  }
}

void I_FinishUpdate(void)
{
  if (noblit || !in_graphics_mode)
    return;

  byte *src = screens[0];

  I_PaletteToRGBA(src, fb_w, fb_h);

  SDL_UpdateTexture(texture, NULL, pixel_buffer, pitch_pixels * (int)sizeof(Uint32));
  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void I_ShutdownGraphics(void)
{
  if (!in_graphics_mode)
    return;
  in_graphics_mode = false;
  in_textmode = true;

  if (texture)
  {
    SDL_DestroyTexture(texture);
    texture = NULL;
  }
  if (rgba_surface)
  {
    SDL_DestroySurface(rgba_surface);
    rgba_surface = NULL;
    pixel_buffer = NULL;
  }
  if (renderer)
  {
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
  }
  if (window)
  {
    SDL_DestroyWindow(window);
    window = NULL;
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

extern boolean setsizeneeded;

// The application icon, as RGBA generated from tools/appicon.png by
// tools/make-icon.ps1. Windows additionally links res/smack.ico in as a
// resource, which is what Explorer and the taskbar read; Linux has no
// equivalent, so setting it here is the only way the window gets branded there.
#include "../res/icon_rgba.h"

static void I_SetWindowIcon(void)
{
  // SDL_SetWindowIcon copies the pixels, so the surface is ours to free -- and
  // the array is static const anyway.
  SDL_Surface *icon =
    SDL_CreateSurfaceFrom(SMACK_ICON_SIZE, SMACK_ICON_SIZE,
                          SDL_PIXELFORMAT_RGBA32,
                          (void *) smack_icon_rgba, SMACK_ICON_SIZE * 4);
  if (icon)
    {
      SDL_SetWindowIcon(window, icon);
      SDL_DestroySurface(icon);
    }
}

static void I_CreateWindowAndRenderer(void)
{
  // The render framebuffer is SCREENWIDTH<<hires x SCREENHEIGHT<<hires
  // (640x400 in the default hi-res mode) and is nearest-neighbour stretched to
  // the window. Window size comes from, in order of precedence:
  //
  //   -geom WxH  ->  -2/-3/-4  ->  SMACK_SCALE  ->  saved v_width/v_height
  //                                             ->  1x (a 640x400 window)
  //
  // The saved size sits below the command-line options deliberately: those stay
  // one-shot overrides for a single run rather than silently rewriting the
  // config.
  int p;
  boolean explicit_size = false;

  hires_flag = hires;
  fb_w = SCREENWIDTH  << hires;
  fb_h = SCREENHEIGHT << hires;

  scale = 1;
  const char *env_scale = getenv("SMACK_SCALE");
  if (env_scale && atoi(env_scale) >= 1)
  {
    scale = atoi(env_scale);
    explicit_size = true;
  }

  // command-line scale shortcuts override SMACK_SCALE
  if      (M_CheckParm("-4")) { scale = 4; explicit_size = true; }
  else if (M_CheckParm("-3")) { scale = 3; explicit_size = true; }
  else if (M_CheckParm("-2")) { scale = 2; explicit_size = true; }

  win_w = fb_w * scale;
  win_h = fb_h * scale;

  // explicit window geometry (e.g. -geom 1280x800) overrides the scaling
  if ((p = M_CheckParm("-geom")) && p < myargc - 1)
  {
    int gw = 0, gh = 0;
    if (sscanf(myargv[p + 1], "%dx%d", &gw, &gh) == 2 && gw > 0 && gh > 0)
    {
      win_w = gw;
      win_h = gh;
      explicit_size = true;
    }
  }

  // nothing on the command line said otherwise -- use the size we saved last
  // time, if there is one
  if (!explicit_size && v_width > 0 && v_height > 0)
  {
    win_w = v_width;
    win_h = v_height;
  }

  // Record the size only when it did not come from the command line, so
  // -geom/-2/-3/-4/SMACK_SCALE stay one-shot for that run. The exception is a
  // config that has no size yet: then whatever we used is the only size the
  // player has actually seen, so it is the sensible thing to store.
  if (!explicit_size || v_width <= 0 || v_height <= 0)
  {
    v_width  = win_w;
    v_height = win_h;
  }

  if (!window)
  {
    window = SDL_CreateWindow("SMACK!",
                              win_w, win_h,
                              SDL_WINDOW_RESIZABLE |
                              (v_fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    if (!window) I_Error("SDL_CreateWindow failed: %s", SDL_GetError());
    I_SetWindowIcon();
  }

  if (!renderer)
  {
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) I_Error("SDL_CreateRenderer failed: %s", SDL_GetError());
  }
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  if (texture)
  {
    SDL_DestroyTexture(texture);
    texture = NULL;
  }
  texture = SDL_CreateTexture(renderer,
                              pixel_fmt,
                              SDL_TEXTUREACCESS_STREAMING,
                              fb_w, fb_h);
  if (!texture) I_Error("SDL_CreateTexture failed: %s", SDL_GetError());
  // nearest-neighbour so the pixel art stays crisp when scaled up
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  if (rgba_surface)
  {
    SDL_DestroySurface(rgba_surface);
    rgba_surface = NULL;
  }
  rgba_surface = SDL_CreateSurface(fb_w, fb_h, pixel_fmt);
  if (!rgba_surface) I_Error("SDL_CreateSurface failed: %s", SDL_GetError());
  pixel_buffer = (Uint32 *)rgba_surface->pixels;
  pitch_pixels = rgba_surface->pitch / 4;
}

void I_ResetScreen(void)
{
  if (!in_graphics_mode)
  {
    setsizeneeded = true;
    V_Init();
    return;
  }

  I_ShutdownGraphics();
  I_CreateWindowAndRenderer();

  in_graphics_mode = true;
  in_textmode = false;
  mouse_grabbed = false;
  I_UpdateGrab();

  if (automapactive) AM_Start();
  ST_Start();
  if (gamestate == GS_INTERMISSION)
  {
    WI_DrawBackground();
    V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
  }
  Z_CheckHeap();
}

void I_InitGraphicsMode(void)
{
  // Rendering is always hi-res (640x400); lowres support has been removed.
  // SMACK_SCALE=N magnifies the window on top of the render framebuffer;
  // I_CreateWindowAndRenderer() reads it and computes win_w/win_h.
  hires_flag = hires;

  if (!initialised)
  {
    initialised = true;

    pixel_fmt = SDL_PIXELFORMAT_ARGB8888;
    pixel_details = SDL_GetPixelFormatDetails(pixel_fmt);
    if (!pixel_details) I_Error("SDL_GetPixelFormatDetails failed: %s", SDL_GetError());
  }

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    I_Error("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());

  atexit(I_ShutdownGraphics);

  if (M_CheckParm("-nomouse"))
    usemouse = 0;

  I_CreateWindowAndRenderer();

  I_InitJoystick();

  I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
  in_textmode = false;
  in_graphics_mode = true;

  mouse_grabbed = false;
  I_UpdateGrab();
}

void I_ResetVidMode(void)
{
  I_ResetScreen();
}

void I_InitGraphics(void)
{
  static int firsttime = true;
  if (!firsttime) return;
  firsttime = false;

  if (nodrawers) return;

  I_InitGraphicsMode();
  V_ResetMode();
  Z_CheckHeap();
}

void I_CheckVESA(void)
{
}

void I_SetMode(int i)
{
  (void)i;
}

// killough 10/98: disk icon routines -- not used on SDL3
void I_BeginRead(void)   {}
void I_EndRead(void)     {}
void I_InitDiskFlash(void) {}

// Apply v_fullscreen to the live window. Going back to windowed restores the
// saved size, since SDL leaves the window at whatever it was before.
void I_SetFullscreen(void)
{
  if (!window)
    return;

  SDL_SetWindowFullscreen(window, v_fullscreen ? true : false);

  if (!v_fullscreen && v_width > 0 && v_height > 0)
  {
    SDL_SetWindowSize(window, v_width, v_height);
    win_w = v_width;
    win_h = v_height;
  }
}

VARIABLE_INT(v_width,  NULL, 0, 32767, NULL);
VARIABLE_INT(v_height, NULL, 0, 32767, NULL);
VARIABLE_BOOLEAN(v_fullscreen, NULL, onoff);

CONSOLE_VARIABLE(v_width,  v_width,  0) {}
CONSOLE_VARIABLE(v_height, v_height, 0) {}

// The handler runs after the variable has been set, so this both toggles from
// the menu and reacts to `v_fullscreen 1` typed at the console.
CONSOLE_VARIABLE(v_fullscreen, v_fullscreen, 0)
{
  I_SetFullscreen();
}

// Alt+Enter, the convention everywhere else.
CONSOLE_COMMAND(togglefullscreen, 0)
{
  v_fullscreen = !v_fullscreen;
  I_SetFullscreen();
}

void I_Video_AddCommands(void)
{
  C_AddCommand(v_width);
  C_AddCommand(v_height);
  C_AddCommand(v_fullscreen);
  C_AddCommand(togglefullscreen);
}
