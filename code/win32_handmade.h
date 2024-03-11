#if !defined(WIN32_HANDMADE_H)

#include <dsound.h>
#include <windows.h>
#include <xinput.h>

struct win32_offscreen_buffer {
  BITMAPINFO info;
  void *memory;
  int width;
  int height;
  int bytes_per_pixel;
  int pitch;
};

struct win32_window_dimension {
  int height;
  int width;
};

struct win32_sound {
  uint32 samples_per_second;
  uint32 bytes_per_sample;
  uint32 buffer_size;
  uint32 latency_bytes;
  uint64 running_sample_index;
  uint16 safety_bytes;
};

struct win32_game_code {
  HMODULE game_code_dll;
  FILETIME dll_last_write;
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;
  bool is_valid;
};

#define WIN32_HANDMADE_H
#endif