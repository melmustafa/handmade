#include "handmade.h"

#define EXPORT extern "C"

internal void RenderWeirdGradient(game_offscreen_buffer *buffer, int x_offset,
                                  int y_offset) {
  uint32 *pixel = (uint32 *)buffer->memory;
  double x_scale = 255.0 / buffer->width;
  double y_scale = 255.0 / buffer->height;
  for (int y = 0; y < buffer->height; y++) {
    for (int x = 0; x < buffer->width; x++) {
      uint8 blue = (uint8)(((x) + x_offset) * x_scale);
      uint8 green = (uint8)(((y) + y_offset) * y_scale);
      *pixel++ = (blue << 8) | green;
    }
  }
}

internal void RenderPlayer(game_offscreen_buffer *buffer, int player_x,
                           int player_y) {
  uint32 *pixel = (uint32 *)buffer->memory;
  pixel += (player_y * buffer->width) + player_x;
  for (int y = player_y; y < player_y + 50; ++y) {
    for (int x = player_x; x < player_x + 50; ++x) {
      *pixel++ = (255 << 16) | (255 << 8) | (255);
    }
    pixel += buffer->width - 50;
  }
}

EXPORT GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((sizeof(game_state) <= memory->permanent_storage_size));

  game_state *state = (game_state *)memory->permanent_storage;
  if (!memory->initialized) {
#if HANDMADE_INTERNAL
    const char *file_name = __FILE__;

    DebugReadFileResult bitmap_memory =
        memory->DebugPlatformReadEntireFile(file_name);
    memory->DebugPlatformWriteEntireFile(
        "data\\test.cpp", bitmap_memory.file_size, bitmap_memory.content);
    memory->DebugPlatformFreeFileMemory(bitmap_memory.content);
#endif

    state->tone_hz = 256;

    state->player_x = 250;
    state->player_y = 250;

    // TODO(zikkas): This maybe be done better in platform layer
    memory->initialized = true;
  }

  const int MAX_CONTROLLER_COUNT = ArrayCount(input->controllers);
  for (int controller_index = 0; controller_index < MAX_CONTROLLER_COUNT;
       controller_index++) {
    game_controller_input *controller = &input->controllers[controller_index];
    if (!controller->is_connected) {
      continue;
    }
    if (controller->is_analog) {
      // NOTE(zikkas): Analog movement
      state->tone_hz =
          256 + (64.0 * (controller->average_x + controller->average_y));
      // state->x_offset += 4.0 * controller->average_x;
      // state->y_offset += 4.0 * controller->average_y;
    } else {
      state->tone_hz = 256 + (64.0 * (controller->move_up.ended_down -
                                      controller->move_down.ended_down +
                                      controller->move_right.ended_down -
                                      controller->move_left.ended_down));
      if (controller->move_down.ended_down) {
        state->player_y -= 4.0;
        // state->y_offset -= 4;
      }
      if (controller->move_up.ended_down) {
        state->player_y += 4.0;
        // state->y_offset += 4;
      }
      if (controller->move_left.ended_down) {
        state->player_x -= 4.0;
        // state->x_offset -= 4;
      }
      if (controller->move_right.ended_down) {
        state->player_x += 4.0;
        // state->x_offset += 4;
      }
    }
    state->player_x += 4.0 * controller->average_x;
    state->player_y += 4.0 * controller->average_y;
    if (controller->action_up.ended_down) {
      state->player_y += 10;
    }
  }
  RenderWeirdGradient(video_buffer, state->x_offset, state->y_offset);
  RenderPlayer(video_buffer, state->player_x, state->player_y);
}

internal void GameOutputSound(game_state *state,
                              game_sound_output_buffer *sound_buffer) {
  uint16 tone_hz = state->tone_hz;
  float t_sine = state->t_sine;
  int16 tone_volume = 1500;
  uint16 wave_period = sound_buffer->samples_per_second / tone_hz;
  float sine_step = M_PI * 2.0 / wave_period;

  int16 *sample_out = sound_buffer->samples;
  for (int sample_index = 0; sample_index < sound_buffer->sample_count;
       sample_index++) {
    float sin_value = sinf(t_sine) * tone_volume;
    sin_value = 0;
    *sample_out++ = (int16)sin_value;
    *sample_out++ = (int16)sin_value;
    t_sine += sine_step;
    if (t_sine > 2 * M_PI) {
      t_sine -= 2 * M_PI;
    }
  }
  state->t_sine = t_sine;
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  Assert((sizeof(game_state) <= memory->permanent_storage_size));

  game_state *state = (game_state *)memory->permanent_storage;
  GameOutputSound(state, sound_buffer);
}
