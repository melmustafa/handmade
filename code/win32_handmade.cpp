/*
    NOTE(zikkas): Ready components are display, sound, input, and timing
    TODO(zikkas): This is not a final platform layer.
    TODO(zikkas): missing components:
    - Saved game locations
    - Getting a handle for our own executable
    - Asset loading path
    - Raw input (multiple keyboard and gamepads)
    - Threading (launch a thread) and multithreading
    - Sleep/time being period
    - ClipCursor() (multi-monitor support)
    - Fullscreen Support
    - WM_SETCURSOR (control cursor visibility)
    - QueryCancelAutoplay
    - WM_ACTIVATEAPP (when not the active application)
    - Blit Speed improvements
    - Hardware accelerations (OpenGL or Direct3D)
    - GetKeyboardLayout (international WASD support)
    - Missing stuff will be added as we go
*/

#include <stdio.h>

// TODO(zikkas): remove all debug string outputs

#include "handmade.h"
#include "win32_handmade.h"

DWORD buttons_bits[] = {XINPUT_GAMEPAD_DPAD_UP,
                        XINPUT_GAMEPAD_DPAD_DOWN,
                        XINPUT_GAMEPAD_DPAD_LEFT,
                        XINPUT_GAMEPAD_DPAD_RIGHT,
                        XINPUT_GAMEPAD_A,
                        XINPUT_GAMEPAD_B,
                        XINPUT_GAMEPAD_X,
                        XINPUT_GAMEPAD_Y,
                        XINPUT_GAMEPAD_LEFT_SHOULDER,
                        XINPUT_GAMEPAD_RIGHT_SHOULDER,
                        XINPUT_GAMEPAD_START,
                        XINPUT_GAMEPAD_BACK};

global_variable bool global_running;
global_variable int64 performance_frequency_count;
global_variable win32_offscreen_buffer video_buffer;
global_variable LPDIRECTSOUNDBUFFER sound_buffer;

// Note(zikkas): This is our support for x_input get and set states
#define X_INPUT_GET_STATE(name)                                                \
  DWORD WINAPI name(DWORD controller_index, XINPUT_STATE *gamepad_state)
typedef X_INPUT_GET_STATE(x_input_get_state);
internal X_INPUT_GET_STATE(x_input_get_state_stub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = x_input_get_state_stub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name)                                                \
  DWORD WINAPI name(DWORD controller_index, XINPUT_VIBRATION *vibrations)
typedef X_INPUT_SET_STATE(x_input_set_state);
internal X_INPUT_SET_STATE(x_input_set_state_stub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = x_input_set_state_stub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput() {
  HMODULE x_input_library = LoadLibraryA("xinput1_4.dll");
  if (!x_input_library) {
    x_input_library = LoadLibraryA("xinput1_3.dll");
  }
  if (x_input_library) {
    XInputGetState =
        (x_input_get_state *)GetProcAddress(x_input_library, "XInputGetState");
    XInputSetState =
        (x_input_set_state *)GetProcAddress(x_input_library, "XInputSetState");
  }
}

internal FILETIME Win32GetLastWriteTime(char *file_name) {
  FILETIME last_write = {};
  WIN32_FIND_DATAA find_data = {};
  HANDLE find_handle = FindFirstFileA(file_name, &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    last_write = find_data.ftLastWriteTime;
    FindClose(find_handle);
  }
  return last_write;
}

internal win32_game_code Win32LoadGameCode(char *source_dll) {
  win32_game_code result = {};

  char temp_dll[] = "bin/win32/temp.dll";

  CopyFileA(source_dll, temp_dll, FALSE);
  result.game_code_dll = LoadLibraryA(temp_dll);

  if (result.game_code_dll) {
    result.dll_last_write = Win32GetLastWriteTime(source_dll);
    result.UpdateAndRender = (game_update_and_render *)GetProcAddress(
        result.game_code_dll, "GameUpdateAndRender");

    result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(
        result.game_code_dll, "GameGetSoundSamples");

    result.is_valid = (result.UpdateAndRender && result.GetSoundSamples);
  }

  if (!result.is_valid) {
    result.UpdateAndRender = GameUpdateAndRenderStub;
    result.GetSoundSamples = GameGetSoundSamplesStub;
  }

  return result;
}

internal void Win32UnloadGameCode(win32_game_code *game) {
  if (game->game_code_dll) {
    FreeLibrary(game->game_code_dll);
    game->game_code_dll = 0;
  }
  game->GetSoundSamples = nullptr;
  game->UpdateAndRender = nullptr;
  game->is_valid = false;
}

internal float Win32ProcessXInputStickValue(short stick_value,
                                            short dead_zone) {
  float result = 0;
  if (stick_value < -dead_zone || stick_value > dead_zone) {
    result = stick_value / (32767.0 + (stick_value < 0));
  }
  return result;
}

internal void Win32ProcessXInputDigitalButton(game_button_state *state,
                                              WORD buttons_state,
                                              WORD button_bits) {
  bool down = state->ended_down;
  state->ended_down = (buttons_state & button_bits);
  state->half_transition_count = (state->ended_down != down) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state *state,
                                          bool is_down) {
  state->ended_down = is_down;
  ++state->half_transition_count;
}

#define D_SOUND_CREATE(name)                                                   \
  HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef D_SOUND_CREATE(d_sound_create);

internal void Win32InitDSound(HWND window, int sample_per_second,
                              int buffer_size) {
  HMODULE d_sound_library = LoadLibraryA("dsound.dll");
  if (d_sound_library) {
    d_sound_create *DirectSoundCreate =
        (d_sound_create *)GetProcAddress(d_sound_library, "DirectSoundCreate");
    LPDIRECTSOUND direct_sound;
    if (DirectSoundCreate &&
        SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0))) {
      WAVEFORMATEX wave_format = {};
      wave_format.wFormatTag = WAVE_FORMAT_PCM;
      wave_format.nChannels = 2;
      wave_format.nSamplesPerSec = sample_per_second;
      wave_format.wBitsPerSample = 16;
      wave_format.nBlockAlign =
          wave_format.nChannels * wave_format.wBitsPerSample / 8;
      wave_format.nAvgBytesPerSec =
          wave_format.nBlockAlign * wave_format.nSamplesPerSec;
      wave_format.cbSize = 0;

      if (SUCCEEDED(
              direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
        DSBUFFERDESC buffer_description = {};
        buffer_description.dwSize = sizeof(buffer_description);
        buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

        LPDIRECTSOUNDBUFFER primary_sound_buffer;
        if (SUCCEEDED(direct_sound->CreateSoundBuffer(
                &buffer_description, &primary_sound_buffer, NULL))) {
          OutputDebugString(TEXT("Primary buffer navigated\n"));
          if (SUCCEEDED(primary_sound_buffer->SetFormat(&wave_format))) {
            OutputDebugString(TEXT("Primary buffer format navigated\n"));
          } else {
            OutputDebugString(TEXT("Primary buffer format NOT NAVIGATED\n"));
          }
        } else {
          OutputDebugString(TEXT("Primary buffer NOT NAVIGATED\n"));
        }

        buffer_description = {};
        buffer_description.dwSize = sizeof(buffer_description);
        buffer_description.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
        buffer_description.dwBufferBytes = buffer_size;
        buffer_description.lpwfxFormat = &wave_format;

        if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description,
                                                      &sound_buffer, NULL))) {
          OutputDebugString(TEXT("Sound buffer navigated\n"));
          // TODO(zikkas): create sound buffer
        } else {
          OutputDebugString(TEXT("Buffer NOT NAVIGATED\n"));
          // TODO(zikkas): errors
        }
      } else {
        // TODO(zikkas): errors
      }

    } else {
      // TODO(zikkas): errors;
      OutputDebugString(TEXT("Sound NOT NAVIGATED"));
    }
  }
}

#if HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(DebugPlatformFreeFileMemory) {
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DebugPlatformReadEntireFile) {
  DWORD bytes_read;
  HANDLE file_handle =
      CreateFileA(FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, NULL);
  DebugReadFileResult result = {};
  if (file_handle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(file_handle, &file_size)) {
      // TODO(zikkas): review this part
      result.file_size = SafeTruncateUInt64(file_size.QuadPart);
      result.content = VirtualAlloc(0, result.file_size,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (result.content) {
        if (ReadFile(file_handle, result.content, result.file_size, &bytes_read,
                     NULL) &&
            (result.file_size == bytes_read)) {
          // NOTE(zikkas): File read navigated
        } else {
          // TODO(zikkas): LOG: Could not read from the file
          DebugPlatformFreeFileMemory(result.content);
          result.content = NULL;
        }
      } else {
        // TODO(zikkas): LOG: Pointer was not created
      }
    } else {
      // TODO(zikkas): LOG: Could not get the file size
    }
    CloseHandle(file_handle);
  } else {
    // TODO(zikkas): LOG: Could not create the file handle
  }
  return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DebugPlatformWriteEntireFile) {
  bool result = false;
  HANDLE file_handle = CreateFileA(FILE_NAME, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file_handle != INVALID_HANDLE_VALUE) {
    DWORD bytes_written;
    if (WriteFile(file_handle, memory, size, &bytes_written, NULL)) {
      // NOTE(zikkas): Check wether all bytes were written
      result = (bytes_written == size);
    } else {
      // TODO(zikkas): LOG: Couldn't write
    }
    CloseHandle(file_handle);
  } else {
    // TODO(zikkas): LOG: Could not create the file handle
  }
  return result;
}
#endif

internal void Win32ClearSoundBuffer(win32_sound *sound) {
  VOID *region1;
  DWORD region1_size;
  VOID *region2;
  DWORD region2_size;
  if (SUCCEEDED(sound_buffer->Lock(0, sound->buffer_size, &region1,
                                   &region1_size, &region2, &region2_size,
                                   0))) {
    int8 *buffer = (int8 *)region1;
    for (DWORD index = 0; index < region1_size; index++) {
      *buffer++ = 0;
    }
    buffer = (int8 *)region2;
    for (DWORD index = 0; index < region2_size; index++) {
      *buffer++ = 0;
    }
    sound_buffer->Unlock(region1, region1_size, region2, region2_size);
  }
}

internal win32_window_dimension Win32GetWindowDimension(HWND window) {
  win32_window_dimension result;

  RECT window_rectangle;
  GetClientRect(window, &window_rectangle);
  result.width = window_rectangle.right - window_rectangle.left;
  result.height = window_rectangle.bottom - window_rectangle.top;

  return result;
}

internal void Win32DisplayBuffer(HDC device_context,
                                 win32_offscreen_buffer *buffer,
                                 int window_width, int window_height) {
  StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                buffer->width, buffer->height, buffer->memory, &buffer->info,
                DIB_RGB_COLORS, SRCCOPY);
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width,
                                    int height) {
  if (buffer->memory) {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;

  buffer->bytes_per_pixel = 4;
  buffer->pitch = buffer->bytes_per_pixel * width;

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = width;
  /*
   * Note(zikkas): the negative value in
   * the height tells windows that the
   * first pixel is the top-left pixel
   * not the bottom left one.
   */
  buffer->info.bmiHeader.biHeight = height;
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = buffer->bytes_per_pixel * 8;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  int bitmap_memory_size =
      buffer->width * buffer->height * buffer->bytes_per_pixel;
  buffer->memory =
      VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

internal LRESULT Win32MainWindowCallback(HWND window, UINT message,
                                         WPARAM WParam, LPARAM LParam) {
  LRESULT result = 0;

  switch (message) {
  case WM_SYSKEYUP:
  case WM_SYSKEYDOWN:
  case WM_KEYUP:
  case WM_KEYDOWN: {
    Assert(!"Keyboard input came in through a callback");
  } break;
  case WM_DESTROY: {
    // TODO(zikkas): check the error and recreate the window?
    global_running = false;
  } break;
  case WM_CLOSE: {
    // TODO(zikkas): check the flow and suggest to save progress?
    global_running = false;
  } break;
  case WM_ACTIVATEAPP: {
    OutputDebugString(TEXT("WM_ACTIVATEAPP\n"));
  } break;
  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC device_context = BeginPaint(window, &paint);
    win32_window_dimension dimension = Win32GetWindowDimension(window);
    Win32DisplayBuffer(device_context, &video_buffer, dimension.width,
                       dimension.height);
    EndPaint(window, &paint);
  } break;
  default: {
    result = DefWindowProc(window, message, WParam, LParam);
  } break;
  }

  return result;
}

internal void Win32FillSoundBuffer(win32_sound *sound_information,
                                   DWORD lock_offset, DWORD bytes_to_write,
                                   game_sound_output_buffer *sound_output) {
  VOID *region1;
  DWORD region1_size;
  VOID *region2;
  DWORD region2_size;

  if (SUCCEEDED(sound_buffer->Lock(lock_offset, bytes_to_write, &region1,
                                   &region1_size, &region2, &region2_size,
                                   0))) {
    int16 *sample_out = (int16 *)region1;
    int16 *sample_in = sound_output->samples;
    DWORD region1_sample_count =
        region1_size / sound_information->bytes_per_sample;
    for (DWORD sample_index = 0; sample_index < region1_sample_count;
         sample_index++) {
      *sample_out++ = *sample_in++;
      *sample_out++ = *sample_in++;
      sound_information->running_sample_index++;
    }
    sample_out = (int16 *)region2;
    DWORD region2_sample_count =
        region2_size / sound_information->bytes_per_sample;
    for (DWORD sample_index = 0; sample_index < region2_sample_count;
         sample_index++) {
      *sample_out++ = *sample_in++;
      *sample_out++ = *sample_in++;
      sound_information->running_sample_index++;
    }
    sound_buffer->Unlock(region1, region1_size, region2, region2_size);
  }
}

inline LARGE_INTEGER Win32GetWallClock(void) {
  LARGE_INTEGER result;
  QueryPerformanceCounter(&result);
  return result;
}

internal inline float Win32GetElapsedSeconds(LARGE_INTEGER start,
                                             LARGE_INTEGER end) {
  float result = (double)(end.QuadPart - start.QuadPart) /
                 (double)performance_frequency_count;
  return result;
}

internal void Win32ProcessPendingMessages(game_controller_input *keyboard) {
  MSG message;

  while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
    switch (message.message) {
    case WM_QUIT: {
      global_running = false;
    } break;
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_KEYDOWN: {
      keyboard->is_connected = true;
      uint32 vk_code = message.wParam;
      bool was_down = ((message.lParam & (1 << 30)) != 0);
      bool is_down = ((message.lParam & (1 << 31)) == 0);
      // Win32ProcessKeyboardMessage(&keyboard->a_button, is_down);
      // Win32ProcessKeyboardMessage(&keyboard->x_button, is_down);
      // Win32ProcessKeyboardMessage(&keyboard->left_shoulder,
      // is_down);
      // Win32ProcessKeyboardMessage(&keyboard->right_shoulder,
      // is_down);
      // Win32ProcessKeyboardMessage(&keyboard->start_button,
      // is_down); Win32ProcessKeyboardMessage(&keyboard->back_button,
      // is_down);
      if (was_down != is_down) {
        if (vk_code == 'W') {
          OutputDebugString(TEXT("W\n"));
        }
        if (vk_code == 'S') {
          OutputDebugString(TEXT("S\n"));
        }
        if (vk_code == 'A') {
          OutputDebugString(TEXT("A\n"));
        }
        if (vk_code == 'D') {
          OutputDebugString(TEXT("D\n"));
        }
        if (vk_code == VK_UP) {
          Win32ProcessKeyboardMessage(&keyboard->move_up, is_down);
          // // OutputDebugString(TEXT("UP\n"));
        }
        if (vk_code == VK_DOWN) {
          Win32ProcessKeyboardMessage(&keyboard->move_down, is_down);
          // // OutputDebugString(TEXT("Down\n"));
        }
        if (vk_code == VK_LEFT) {
          Win32ProcessKeyboardMessage(&keyboard->move_left, is_down);
          // // OutputDebugString(TEXT("Left\n"));
        }
        if (vk_code == VK_RIGHT) {
          Win32ProcessKeyboardMessage(&keyboard->move_right, is_down);
          // // OutputDebugString(TEXT("Right\n"));
        }
        if (vk_code == 'Q') {
          Win32ProcessKeyboardMessage(&keyboard->action_up, is_down);
          // // OutputDebugString(TEXT("Q\n"));
        }
        if (vk_code == 'E') {
          Win32ProcessKeyboardMessage(&keyboard->action_right, is_down);
          // // OutputDebugString(TEXT("E\n"));
        }
        if (vk_code == VK_SPACE && is_down) {
          keyboard->is_connected = false;
          OutputDebugString(TEXT("SpaceBar\n"));
        }
        if (vk_code == VK_ESCAPE) {
          Win32ProcessKeyboardMessage(&keyboard->back_button, is_down);
          // // OutputDebugString(TEXT("Escape\n"));
        }
      }
      bool alt_is_down = (message.lParam & (1 << 29)) != 0;
      if (alt_is_down && (vk_code == VK_F4)) {
        global_running = false;
      }
    } break;
    default: {
      TranslateMessage(&message);
      DispatchMessage(&message);
    } break;
    }
  }
}

#if HANDMADE_INTERNAL
struct win32_Debug_time_marker {
  DWORD play;
  DWORD write;
};

internal void Win32DebugDrawVertical(win32_offscreen_buffer *video, int x,
                                     int top, int bottom, uint32 color) {
  uint8 *pixel = (uint8 *)(video->memory) + x * video->bytes_per_pixel +
                 top * video->pitch;
  for (int y = top; y < bottom; ++y) {
    *(uint32 *)pixel = color;
    pixel += video->pitch;
  }
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *video,
                                    int marker_size,
                                    win32_Debug_time_marker *play_cursor,
                                    int current_marker, win32_sound *sound,
                                    float seconds_per_frame) {
  int pad_x = 64;
  int pad_y = 64;

  float c = (float)(video->width - 2 * pad_x) / (float)sound->buffer_size;
  for (int marker_index = 0; marker_index < marker_size; marker_index++) {
    int bottom = video->height - pad_y;
    int top = video->height - (2 * pad_y);

    int x = pad_x + (int)(c * play_cursor[marker_index].play);
    DWORD color = 0x99999999;
    if (current_marker == marker_index) {
      color = 0xFFFFFFFF;
      bottom = video->height - (3 * pad_y);
      top = video->height - (4 * pad_y);
    }
    Win32DebugDrawVertical(video, x, top, bottom, color);
    x = pad_x + (int)(c * play_cursor[marker_index].write);
    color = 0x99990000;
    if (current_marker == marker_index) {
      color = 0xFFFF0000;
    }
    Win32DebugDrawVertical(video, x, top, bottom, color);
  }
}
#endif

int WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine,
            int ShowCmd) {
  {
    LARGE_INTEGER performance_frequency;
    QueryPerformanceFrequency(&performance_frequency);
    performance_frequency_count = performance_frequency.QuadPart;
  }

  Win32LoadXInput();

  WNDCLASSA window_class = {};
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.lpfnWndProc = Win32MainWindowCallback;
  window_class.hInstance = Instance;
  window_class.lpszClassName = "HandmadeHeroWindowClass";
  // // window_class.hIcon =
  // // SOME_RELATIVE_PATH_TO_ICON;

  uint8 monitor_refresh_rate = 60;
  uint8 game_refresh_rate = monitor_refresh_rate / 2;
  float seconds_per_frame = 1.0 / (float)game_refresh_rate;
  timeBeginPeriod(1);

  if (RegisterClassA(&window_class)) {
    HWND window = CreateWindowExA(0, window_class.lpszClassName, "HandmadeHero",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 0, 0, Instance, 0);
    if (window) {
      global_running = true;

      // * Note(zikkas): I am using cli debugger so using a certain location in
      // * the memory is not beneficial for me
      // // #if HANDMADE_INTERNAL
      // //       LPVOID base_address = Terabytes(2);
      // // #else
      // //       LPVOID base_address = NULL;
      // // #endif

      game_memory memory = {};
      memory.permanent_storage_size = Megabytes(64);
      memory.permanent_storage =
          VirtualAlloc(0, memory.permanent_storage_size,
                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

      memory.transient_storage_size = Gigabytes(4);
      memory.transient_storage =
          VirtualAlloc(0, memory.transient_storage_size,
                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#if HANDMADE_INTERNAL
      memory.DebugPlatformFreeFileMemory =
          (debug_platform_free_file_memory *)DebugPlatformFreeFileMemory;
      memory.DebugPlatformReadEntireFile =
          (debug_platform_read_entire_file *)DebugPlatformReadEntireFile;
      memory.DebugPlatformWriteEntireFile =
          (debug_platform_write_entire_file *)DebugPlatformWriteEntireFile;
#endif

      win32_sound sound_output = {};

      sound_output.samples_per_second = 48000;
      sound_output.running_sample_index = 0;
      sound_output.bytes_per_sample = sizeof(int16) * 2;
      sound_output.buffer_size =
          sound_output.samples_per_second * sound_output.bytes_per_sample;
      sound_output.latency_bytes = sound_output.samples_per_second *
                                   sound_output.bytes_per_sample /
                                   game_refresh_rate;
      sound_output.safety_bytes = sound_output.latency_bytes / 2;
      int16 *samples =
          (int16 *)VirtualAlloc(0, sound_output.buffer_size,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      Win32InitDSound(window, sound_output.samples_per_second,
                      sound_output.buffer_size);
      Win32ClearSoundBuffer(&sound_output);
      sound_buffer->Play(0, 0, DSBPLAY_LOOPING);

      HDC device_context = GetDC(window);
      Win32ResizeDIBSection(&video_buffer, 1920, 1080);
#if HANDMADE_INTERNAL
      unsigned short Debug_last_marker_index = 0;
      win32_Debug_time_marker Debug_last_marker[game_refresh_rate / 4] = {};
#endif
      LARGE_INTEGER last_counter = Win32GetWallClock();
      uint64 last_timestamp = __rdtsc();

      bool valid_sound = false;

      char source_dll[] = "bin/win32/handmade.dll";
      win32_game_code game = Win32LoadGameCode(source_dll);

      game_input input = {};
      while (global_running) {
        FILETIME current_write = Win32GetLastWriteTime(source_dll);
        if (CompareFileTime(&game.dll_last_write, &current_write) != 0) {
          Win32UnloadGameCode(&game);
          game = Win32LoadGameCode(source_dll);
        }

        Win32ProcessPendingMessages(&input.controllers[0]);

        const int MAX_CONTROLLER_COUNT =
            Min((int)XUSER_MAX_COUNT, (int)ArrayCount(input.controllers) - 1);
        const int NUMBER_OF_BUTTONS = ArrayCount(buttons_bits);
        for (DWORD controller_index = 0;
             (int)controller_index < MAX_CONTROLLER_COUNT; controller_index++) {
          XINPUT_STATE controller_state;
          game_controller_input *controller =
              &input.controllers[controller_index + 1];
          if (XInputGetState(controller_index, &controller_state) ==
              ERROR_SUCCESS) {
            // TODO(zikkas): check XINPUT_STATE dwPacketNumber
            XINPUT_GAMEPAD *pad = &(controller_state.Gamepad);
            controller->is_connected = true;
            for (int button = 0; button <= NUMBER_OF_BUTTONS; ++button) {
              Win32ProcessXInputDigitalButton(&controller->buttons[button],
                                              pad->wButtons,
                                              buttons_bits[button]);
            }

            controller->is_analog = true;
            controller->average_x = Win32ProcessXInputStickValue(
                pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            controller->average_y = Win32ProcessXInputStickValue(
                pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            if (controller->move_up.ended_down ||
                controller->move_down.ended_down) {
              controller->average_y = controller->move_up.ended_down -
                                      controller->move_down.ended_down;
            }
            if (controller->move_right.ended_down ||
                controller->move_left.ended_down) {
              controller->average_x = controller->move_right.ended_down -
                                      controller->move_left.ended_down;
            }
            XINPUT_VIBRATION vibrations;
            vibrations.wLeftMotorSpeed = 0;
            vibrations.wRightMotorSpeed = 0;

            XInputSetState(controller_index, &vibrations);
          } else {
            // NOTE(zikkas): controller is not available
            controller->is_connected = false;
          }
        }
        if (input.controllers[0].back_button.ended_down ||
            input.controllers[1].back_button.ended_down) {
          global_running = false;
        }

        /*
          Note(zikkas):
          * Here is how sound output works
          *
          * We define a safety value that is the number of sample we think our
          * game update loop vary by (let's say up to 2 ms)
          *
          * When we wake up to write audio, we will look and see what the play
          * cursor position is and we will forecast ahead where we think the
          * play cursor will be on the next frame boundary.
          *
          * We will then look if the write cursor is before that by at least the
          * safety value. If it is, the target fill postion is that frame
          * boundary plus one frame. This gives us a perfect audio sync in the
          * case of a card that has low enough latency.
          *
          * If the write cursor is _after_ that safety margin, then we
          * assume we can never sync the audio perfectly, so we will write one
          * frame's worth plus the safety margin's worth of guard samples.
         */

        game_offscreen_buffer buffer = {};
        buffer.memory = video_buffer.memory;
        buffer.bytes_per_pixel = video_buffer.bytes_per_pixel;
        buffer.height = video_buffer.height;
        buffer.width = video_buffer.width;

        game.UpdateAndRender(&memory, &input, &buffer);

        DWORD play_cursor, write_cursor;
        if (sound_buffer->GetCurrentPosition(&play_cursor, &write_cursor) ==
            DS_OK) {
          if (!valid_sound) {
            sound_output.running_sample_index =
                write_cursor / sound_output.bytes_per_sample;
            valid_sound = true;
          }

          DWORD lock_offset = (sound_output.running_sample_index *
                               sound_output.bytes_per_sample) %
                              sound_output.buffer_size;

          DWORD expected_frame_boundary_byte =
              play_cursor + sound_output.latency_bytes;

          DWORD target_cursor;

          DWORD safe_write_cursor = write_cursor;
          if (safe_write_cursor < play_cursor) {
            safe_write_cursor += sound_output.buffer_size;
          }
          Assert(safe_write_cursor >= play_cursor);
          safe_write_cursor += sound_output.safety_bytes;
          bool audio_card_is_latent =
              (safe_write_cursor >= expected_frame_boundary_byte);

          if (audio_card_is_latent) {
            target_cursor = (write_cursor + sound_output.latency_bytes +
                             sound_output.safety_bytes);
          } else {
            target_cursor =
                (sound_output.latency_bytes + expected_frame_boundary_byte);
          }
          target_cursor = target_cursor % sound_output.buffer_size;

          DWORD bytes_to_write =
              target_cursor - lock_offset +
              (sound_output.buffer_size * (lock_offset > target_cursor));

          game_sound_output_buffer sound_output_buffer = {};
          sound_output_buffer.samples_per_second =
              sound_output.samples_per_second;
          sound_output_buffer.samples = samples;
          sound_output_buffer.sample_count =
              bytes_to_write / sound_output.bytes_per_sample;
          game.GetSoundSamples(&memory, &sound_output_buffer);

          Win32FillSoundBuffer(&sound_output, lock_offset, bytes_to_write,
                               &sound_output_buffer);

#if HANDMADE_INTERNAL
          sound_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
          char buffer[255];
          DWORD unwrapped_write_cursor = write_cursor;
          if (unwrapped_write_cursor < play_cursor) {
            unwrapped_write_cursor += sound_output.buffer_size;
          }
          DWORD bytes_between_cursors = unwrapped_write_cursor - play_cursor;
          float audio_latency_seconds = ((float)bytes_between_cursors /
                                         (float)sound_output.bytes_per_sample) /
                                        (float)sound_output.samples_per_second;
          sprintf_s(buffer,
                    "lock_offset: %d, target_cursor: %d, bytes_to_write: %d, "
                    "play_cursor: %d, write_cursor: %d, audio_latency: %fms",
                    lock_offset, target_cursor, bytes_to_write, play_cursor,
                    write_cursor, audio_latency_seconds * 1000);
          OutputDebugString(buffer);
#endif
        } else {
          valid_sound = false;
        }

        {
          LARGE_INTEGER end_counter = Win32GetWallClock();
          float seconds_elapsed_for_last_frame =
              Win32GetElapsedSeconds(last_counter, end_counter);

          if (seconds_elapsed_for_last_frame < seconds_per_frame) {
            Sleep((DWORD)(1000.0 * (seconds_per_frame -
                                    seconds_elapsed_for_last_frame)));
          } else if (seconds_elapsed_for_last_frame > seconds_per_frame) {
            // TODO(zikkas): missed frame
          }
        }

        LARGE_INTEGER end_counter = Win32GetWallClock();
        float ms_per_frame =
            1000.0 * Win32GetElapsedSeconds(last_counter, end_counter);

        {
          win32_window_dimension dimension = Win32GetWindowDimension(window);

#if HANDMADE_INTERNAL
          {
            Assert((Debug_last_marker_index < ArrayCount(Debug_last_marker)));
            win32_Debug_time_marker marker = {};
            marker.play = play_cursor;
            marker.write = write_cursor;
            Debug_last_marker[Debug_last_marker_index++] = marker;
            Win32DebugSyncDisplay(
                &video_buffer, ArrayCount(Debug_last_marker), Debug_last_marker,
                Debug_last_marker_index - 1, &sound_output, seconds_per_frame);
            Debug_last_marker_index %= ArrayCount(Debug_last_marker);
          }
#endif

          Win32DisplayBuffer(device_context, &video_buffer, dimension.width,
                             dimension.height);
        }

        uint64 end_timestamp = __rdtsc();

#if HANDMADE_SLOW
        // TODO(zikkas): Display elapsed time

        float cycle_elapsed = end_timestamp - last_timestamp;
        float fps = 1000.0 / ms_per_frame;

        char debug_buffer[256];
        sprintf(debug_buffer, "%f ms/f, %f fps, %f mc/f\n", ms_per_frame, fps,
                cycle_elapsed / 1000.0 / 1000.0);
        // OutputDebugString(debug_buffer);
#endif

        last_timestamp = end_timestamp;
        last_counter = end_counter;
      }
      ReleaseDC(window, device_context);
    } else {
      // todo: Logging
    }
  } else {
    // todo: Logging
  }

  return 0;
}