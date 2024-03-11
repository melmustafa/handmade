#if !defined(HANDMADE_H)

// TODO(zikkas): implement all needed functions internally
// - Needed functions for now:
// - sinf()
#include <math.h>
#include <stdint.h>

/*
  NOTE(zikkas): HANDMADE_INTERNAL options:
  * 0: Build for public release
  * 1: Build for developer only
*/

/*
  NOTE(zikkas): HANDMADE_SLOW options:
  * 0: All slow debug expressions are ignored
  * 1: All slow debug expressions are allowed
*/

#define int8 int8_t
#define int16 int16_t
#define int32 int32_t
#define int64 int64_t
#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define internal static
#define local_persist static
#define global_variable static

#if HANDMADE_INTERNAL
struct DebugReadFileResult {
  void *content;
  uint32 file_size;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)                                  \
  DebugReadFileResult name(const char *FILE_NAME)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name)                                 \
  bool name(const char *FILE_NAME, uint32 size, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

#if HANDMADE_SLOW
#define Assert(expression)                                                     \
  if (!(expression)) {                                                         \
    *(int *)0 = 0;                                                             \
  }
#else
#define Assert(expression)
#endif

#define Kilobytes(value) (value * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024LL)
#define Terabytes(value) (Gigabytes(value) * 1024LL)
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
#define Min(x, y) ((x < y) ? x : y)
#define Max(x, y) ((x > y) ? x : y)

inline uint32 SafeTruncateUInt64(uint64 value) {
  Assert((value <= UINT32_MAX));
  uint32 result = (uint32)(value);
  return result;
}

struct game_memory {
  bool initialized;
  uint64 permanent_storage_size; // * in bytes
  void *permanent_storage;       // * Required to be cleared at startup

  uint64 transient_storage_size; // * in bytes
  void *transient_storage;       // * Required to be cleared at startup

#if HANDMADE_INTERNAL
  debug_platform_free_file_memory *DebugPlatformFreeFileMemory;
  debug_platform_read_entire_file *DebugPlatformReadEntireFile;
  debug_platform_write_entire_file *DebugPlatformWriteEntireFile;
#endif
};

struct game_offscreen_buffer {
  void *memory;
  int bytes_per_pixel;
  int width;
  int height;
};

struct game_sound_output_buffer {
  int samples_per_second;
  int sample_count;
  int16 *samples;
};

struct game_button_state {
  uint16 half_transition_count;
  bool ended_down;
};
struct game_controller_input {
  bool is_analog;
  bool is_connected;
  float average_x;
  float average_y;

  union {
    game_button_state buttons[12];
    struct {
      game_button_state move_up;
      game_button_state move_down;
      game_button_state move_left;
      game_button_state move_right;

      game_button_state action_up;
      game_button_state action_down;
      game_button_state action_left;
      game_button_state action_right;

      game_button_state left_shoulder;
      game_button_state right_shoulder;

      game_button_state start_button;
      game_button_state back_button;
    };
  };
};

struct game_input {
  game_controller_input controllers[5];
};

struct game_clock {
  float seconds_elapsed;
};

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(game_memory *memory, game_input *input,                            \
            game_offscreen_buffer *video_buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {}

#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(game_memory *memory, game_sound_output_buffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub) {}

struct game_state {
  int x_offset;
  int y_offset;

  int tone_hz;
  float t_sine;

  int player_x;
  int player_y;
};
#define HANDMADE_H
#endif