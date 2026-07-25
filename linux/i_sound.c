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
//      SDL3 sound system. Software SFX mixer: Doom's 8-bit unsigned mono
//      DMX sound lumps are resampled and mixed into a 16-bit stereo SDL3
//      audio stream by a pull callback. Stereo separation and per-channel
//      volume follow the classic linuxdoom formula. Music is still silent.
//
//-----------------------------------------------------------------------------

// pulls in SMMU's doomtype.h (typedef enum {false, true} boolean)
// before SDL3's stdbool macros collide with those names.
#include "../doomstat.h"
#include "../i_sound.h"
#include "../i_system.h"
#include "../w_wad.h"
#include "../g_game.h"
#include "../d_main.h"
#include "../z_zone.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int snd_card = 0, mus_card = 0, detect_voices;

// ---- output format --------------------------------------------------------

#define OUT_FREQ     44100
#define OUT_CHANNELS 2
#define NORM_PITCH   128        // matches s_sound.c: pitch 128 == unaltered

// number of simultaneous mixer voices. S_ layer caps itself at
// default_numChannels (<=128); we keep parity so a channel always maps.
#define MIX_VOICES   128

typedef struct
{
  const unsigned char *data;   // 8-bit unsigned PCM (points into cached lump)
  unsigned int len;            // number of samples
  unsigned int pos;            // 16.16 fixed-point playback cursor
  unsigned int base_step;      // 16.16 native-rate ratio, before pitch
  unsigned int step;           // 16.16 fixed-point samples/output-frame
  int          leftvol;        // 0..127
  int          rightvol;       // 0..127
  int          handle;         // unique id handed back to the S_ layer; 0=free
  int          active;
} voice_t;

static voice_t          voices[MIX_VOICES];
static int              next_handle = 1;

static SDL_AudioStream *audio_stream    = NULL;
static int              audio_initialised = 0;

// ---- volume / separation --------------------------------------------------

// vol: 0..snd_SfxVolume (0..15).  sep: 0..255 (128 = centred).
// Produces left/right volumes in 0..127 using the x^2 panning law.
static void calc_volumes(int vol, int sep, int *left, int *right)
{
  int v = (vol * 127) / 15;             // scale 0..15 -> 0..127
  int s;

  if (v < 0)   v = 0;
  if (v > 127) v = 127;

  sep += 1;                             // 1..256
  *left  = v - ((v * sep * sep) >> 16);
  s = 257 - sep;
  *right = v - ((v * s * s) >> 16);

  if (*left  < 0)   *left  = 0;
  if (*left  > 127) *left  = 127;
  if (*right < 0)   *right = 0;
  if (*right > 127) *right = 127;
}

// ---- sfx lump loading -----------------------------------------------------

// Cache the raw DMX lump for this sfx (once) and hand back the sample data,
// its length and native sample rate parsed from the 8-byte header.
static const unsigned char *lock_sfx(sfxinfo_t *sfx, unsigned int *len, int *rate)
{
  const unsigned char *lump;
  unsigned int lumplen, nsamp;

  if (!sfx->data)
  {
    int lumpnum = I_GetSfxLumpNum(sfx);
    if (lumpnum < 0)
      lumpnum = W_GetNumForName("dspistol");
    sfx->length = W_LumpLength(lumpnum);
    sfx->data   = W_CacheLumpNum(lumpnum, PU_STATIC);
  }

  lump    = (const unsigned char *)sfx->data;
  lumplen = (unsigned int)sfx->length;

  if (lumplen <= 8)                     // malformed / empty
    return NULL;

  // DMX header: [0..1] format, [2..3] sample rate, [4..7] sample count
  *rate = lump[2] | (lump[3] << 8);
  if (*rate < 8000 || *rate > 48000)
    *rate = 11025;

  nsamp = lump[4] | (lump[5] << 8) | (lump[6] << 16) | ((unsigned)lump[7] << 24);
  if (nsamp > lumplen - 8)
    nsamp = lumplen - 8;

  *len = nsamp;
  return lump + 8;
}

// ---- SFX API --------------------------------------------------------------

void I_SetChannels(void)              { }
void I_CacheSound(sfxinfo_t *sound)   { (void)sound; }

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
  char namebuf[16];
  sprintf(namebuf, "ds%s", sfx->name);
  return W_CheckNumForName(namebuf);
}

int I_StartSound(sfxinfo_t *sound, int vol, int sep, int pitch, int pri)
{
  const unsigned char *data;
  unsigned int len;
  int rate, i, slot = -1, handle;
  unsigned int base_step, step;

  (void)pri;

  if (!audio_initialised || !sound)
    return 0;

  data = lock_sfx(sound, &len, &rate);
  if (!data || !len)
    return 0;

  base_step = (unsigned int)(((uint64_t)rate << 16) / OUT_FREQ);
  if (base_step == 0)
    base_step = 1;
  step = (unsigned int)(((uint64_t)base_step * (unsigned)pitch) / NORM_PITCH);
  if (step == 0)
    step = 1;

  handle = next_handle++;
  if (next_handle <= 0)                 // wrap: never reuse 0
    next_handle = 1;

  SDL_LockAudioStream(audio_stream);

  for (i = 0; i < MIX_VOICES; i++)
    if (!voices[i].active) { slot = i; break; }

  if (slot >= 0)
  {
    voice_t *v = &voices[slot];
    v->data = data;
    v->len  = len;
    v->pos  = 0;
    v->base_step = base_step;
    v->step = step;
    calc_volumes(vol, sep, &v->leftvol, &v->rightvol);
    v->handle = handle;
    v->active = 1;
  }

  SDL_UnlockAudioStream(audio_stream);

  return slot >= 0 ? handle : 0;
}

void I_StopSound(int handle)
{
  int i;
  if (!audio_initialised || handle <= 0)
    return;
  SDL_LockAudioStream(audio_stream);
  for (i = 0; i < MIX_VOICES; i++)
    if (voices[i].active && voices[i].handle == handle)
    {
      voices[i].active = 0;
      break;
    }
  SDL_UnlockAudioStream(audio_stream);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
  int i;
  if (!audio_initialised || handle <= 0)
    return;
  SDL_LockAudioStream(audio_stream);
  for (i = 0; i < MIX_VOICES; i++)
    if (voices[i].active && voices[i].handle == handle)
    {
      calc_volumes(vol, sep, &voices[i].leftvol, &voices[i].rightvol);
      // derive from base_step so repeated updates don't compound the pitch
      voices[i].step = (unsigned int)(((uint64_t)voices[i].base_step
                                       * (unsigned)pitch) / NORM_PITCH);
      if (voices[i].step == 0)
        voices[i].step = 1;
      break;
    }
  SDL_UnlockAudioStream(audio_stream);
}

int I_SoundIsPlaying(int handle)
{
  int i;
  if (!audio_initialised || handle <= 0)
    return 0;
  for (i = 0; i < MIX_VOICES; i++)
    if (voices[i].active && voices[i].handle == handle)
      return 1;
  return 0;
}

// callback-driven; nothing to push from the game loop
void I_UpdateSound(void)  { }
void I_SubmitSound(void)  { }

void I_SetSfxVolume(int volume)
{
  snd_SfxVolume = volume;
}

// ---- the mixer ------------------------------------------------------------

// Render `frames` stereo S16 frames, mixing every active voice.
static void mix_frames(Sint16 *out, int frames)
{
  int f, i;

  for (f = 0; f < frames; f++)
  {
    int left = 0, right = 0;

    for (i = 0; i < MIX_VOICES; i++)
    {
      voice_t *v = &voices[i];
      unsigned int idx;
      int s;

      if (!v->active)
        continue;

      idx = v->pos >> 16;
      if (idx >= v->len)
      {
        v->active = 0;
        continue;
      }

      s = (int)v->data[idx] - 128;      // -128..127
      left  += s * v->leftvol;
      right += s * v->rightvol;

      v->pos += v->step;
      if ((v->pos >> 16) >= v->len)
        v->active = 0;
    }

    left  <<= 1;                        // one full-volume voice ~= 0.75 fullscale
    right <<= 1;

    if (left  >  32767) left  =  32767;
    if (left  < -32768) left  = -32768;
    if (right >  32767) right =  32767;
    if (right < -32768) right = -32768;

    *out++ = (Sint16)left;
    *out++ = (Sint16)right;
  }
}

// SDL3 pull callback: produce `additional_amount` bytes of audio on demand.
static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream,
                                   int additional_amount, int total_amount)
{
  static Sint16 buf[2048 * OUT_CHANNELS];
  const int bytes_per_frame = OUT_CHANNELS * (int)sizeof(Sint16);

  (void)userdata; (void)total_amount;

  while (additional_amount > 0)
  {
    int want_frames = additional_amount / bytes_per_frame;
    if (want_frames > 2048)
      want_frames = 2048;
    if (want_frames <= 0)
      break;

    mix_frames(buf, want_frames);
    SDL_PutAudioStreamData(stream, buf, want_frames * bytes_per_frame);
    additional_amount -= want_frames * bytes_per_frame;
  }
}

// ---- init / shutdown ------------------------------------------------------

void I_InitSound(void)
{
  SDL_AudioSpec want;

  if (audio_initialised)
    return;

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
  {
    fprintf(stderr, "I_InitSound: SDL audio init failed: %s\n", SDL_GetError());
    return;
  }

  SDL_zero(want);
  want.freq     = OUT_FREQ;
  want.format   = SDL_AUDIO_S16;        // native-endian signed 16-bit
  want.channels = OUT_CHANNELS;

  audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                           &want, audio_callback, NULL);
  if (!audio_stream)
  {
    fprintf(stderr, "I_InitSound: SDL_OpenAudioDeviceStream failed: %s\n",
            SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return;
  }

  SDL_ResumeAudioStreamDevice(audio_stream);

  audio_initialised = 1;
  snd_card = 1;                         // tell the S_ layer sfx are available
  printf("I_InitSound: %d Hz, %d-bit, %s\n", OUT_FREQ, 16,
         OUT_CHANNELS == 2 ? "stereo" : "mono");
}

void I_ShutdownSound(void)
{
  if (audio_stream)
  {
    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
  }
  if (audio_initialised)
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  audio_initialised = 0;
  snd_card = 0;
}

// ---- MUSIC API (still silent) ---------------------------------------------

void I_InitMusic(void)        { atexit(I_ShutdownMusic); }
void I_ShutdownMusic(void)    { }

void I_SetMusicVolume(int v)  { snd_MusicVolume = v; }

void I_PauseSong(int h)       { (void)h; }
void I_ResumeSong(int h)      { (void)h; }
void I_PlaySong(int h, int l) { (void)h; (void)l; }
void I_StopSong(int h)        { (void)h; }
void I_UnRegisterSong(int h)  { (void)h; }

int I_RegisterSong(void *data)
{
  (void)data;
  return 1;
}

int I_QrySongPlaying(int h)
{
  (void)h;
  return 0;
}

void I_Sound_AddCommands(void)
{
}
