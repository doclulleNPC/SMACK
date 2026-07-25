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

static int           win_w = 1280;
static int           win_h = 800;
static int           scale = 1;     // window magnification applied on top of
                                   // the render framebuffer. SMMU_SCALE=N sets
                                   // it (default 1). With hires the framebuffer
                                   // is already 640x400, so scale=1 -> 640x400.
static int           hires_flag = 0;
static int           fb_w = SCREENWIDTH;   // render framebuffer size, =
static int           fb_h = SCREENHEIGHT;  // SCREEN{WIDTH,HEIGHT} << hires

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

static void post_mouse_motion(int dx, int dy)
{
  event_t ev;
  ev.type  = ev_mouse;
  ev.data1 = 0;
  ev.data2 = dx << 2;
  ev.data3 = -(dy << 2);
  D_PostEvent(&ev);
}

static void post_mouse_button(int sdl_button, boolean down)
{
  int mask = 0;
  if (sdl_button == SDL_BUTTON_LEFT)   mask |= 1;
  if (sdl_button == SDL_BUTTON_RIGHT)  mask |= 2;
  if (sdl_button == SDL_BUTTON_MIDDLE) mask |= 4;
  event_t ev;
  ev.type  = ev_mouse;
  ev.data1 = mask;
  ev.data2 = ev.data3 = 0;
  D_PostEvent(&ev);
}

// ---- I_StartTic / I_GetEvent ---------------------------------------------

void I_GetEvent(void);

void I_StartTic(void)
{
  I_GetEvent();
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
      {
        event_t e;
        e.type = ev_keydown;
        e.data1 = key_escape;
        e.data2 = e.data3 = 0;
        D_PostEvent(&e);
        break;
      }

      case SDL_EVENT_KEY_DOWN:
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

      case SDL_EVENT_WINDOW_RESIZED:
        win_w = ev.window.data1;
        win_h = ev.window.data2;
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

static void I_CreateWindowAndRenderer(void)
{
  // The render framebuffer is SCREENWIDTH<<hires x SCREENHEIGHT<<hires
  // (640x400 in the default hi-res mode). SMMU_SCALE=N further magnifies
  // the window; default 1 so hi-res gives a 640x400 window.
  hires_flag = hires;
  fb_w = SCREENWIDTH  << hires;
  fb_h = SCREENHEIGHT << hires;

  scale = 1;
  const char *env_scale = getenv("SMMU_SCALE");
  if (env_scale && atoi(env_scale) >= 1)
    scale = atoi(env_scale);

  win_w = fb_w * scale;
  win_h = fb_h * scale;

  if (!window)
  {
    window = SDL_CreateWindow("SMMU - Smack My Marine Up",
                              win_w, win_h,
                              SDL_WINDOW_RESIZABLE);
    if (!window) I_Error("SDL_CreateWindow failed: %s", SDL_GetError());
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
  // SMMU_SCALE=N magnifies the window on top of the render framebuffer;
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

void I_Video_AddCommands(void)
{
}
