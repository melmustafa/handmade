#include <stdint.h>
#include <windows.h>

#define internal static
#define global_variable static
#define local_persist static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

// NOTE: this is a local (only works in this file) global variable for now

struct win32_offscreen_buffer {
  BITMAPINFO info;
  void* memory;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
};

struct win32_window_dimension {
  int height;
  int width;
};

// TODO: this is global for now
global_variable bool global_running;
global_variable win32_offscreen_buffer global_back_buffer;

win32_window_dimension Win32GetWindowDimension(HWND window) {
  win32_window_dimension result;

  RECT client_rect;
  GetClientRect(window, &client_rect);
  result.height = client_rect.bottom - client_rect.top;
  result.width = client_rect.right - client_rect.left;

  return result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer buffer, int x_offset, int y_offset) {
  uint8* row = (uint8*)buffer.memory;
  for (int y = 0; y < buffer.height; ++y) {
    uint32* pixel = (uint32*)row;
    for (int x = 0; x < buffer.width; ++x) {
      *pixel = ((x * 255 / buffer.width) + x_offset) | (((y * 255 / buffer.height) + y_offset) << 8);

      pixel++;
    }
    row += buffer.pitch;
  }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer* buffer, int width, int height) {
  if (buffer->memory) {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = 4;

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = buffer->width;
  buffer->info.bmiHeader.biHeight = -buffer->height;
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = 32;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = (buffer->width * buffer->height) * buffer->bytes_per_pixel;
  buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

  buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

internal void Win32DisplayWindow(HDC deviceContext,
                                 win32_offscreen_buffer buffer,
                                 int window_width,
                                 int window_height) {
  // TODO: check other possible stretch modes
  StretchDIBits(deviceContext,
                0,
                0,
                window_width,
                window_height,
                0,
                0,
                buffer.width,
                buffer.height,
                buffer.memory,
                &(buffer.info),
                DIB_RGB_COLORS,
                SRCCOPY);
}

LRESULT MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
    case WM_DESTROY: {
      OutputDebugString("WM_DESTROY\n");
      // TODO: this might be an error
      global_running = false;
    } break;
    case WM_CLOSE: {
      OutputDebugString("WM_CLOSE\n");
      // TODO: handle the interaction with the message
      global_running = false;
    } break;
    case WM_SIZE: {
      OutputDebugString("WM_SIZE\n");
      // TODO: casey is using client rect check the performance penalty for this
      // NOTE: stop resizing and only allow prespecified sizes from inside the game loop
    } break;
    case WM_ACTIVATE: {
      OutputDebugString("WM_ACTIVATE\n");
    } break;
    case WM_PAINT: {
      OutputDebugString("WM_PAINT\n");
      PAINTSTRUCT paint;
      HDC deviceContext = BeginPaint(window, &paint);

      win32_window_dimension dimension = Win32GetWindowDimension(window);
      RenderWeirdGradient(global_back_buffer, 0, 0);
      Win32DisplayWindow(deviceContext, global_back_buffer, dimension.width, dimension.height);

      EndPaint(window, &paint);
    } break;
    default: {
      OutputDebugString("default\n");
      result = DefWindowProc(window, message, wParam, lParam);
    } break;
  }

  return result;
}

int WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd) {
  Win32ResizeDIBSection(&global_back_buffer, 1280, 720);

  WNDCLASS windowClass = {};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = MainWindowCallback;
  windowClass.hInstance = instance;
  // window.hIcon;
  windowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClass(&windowClass)) {
    HWND window = CreateWindowExA(0,
                                  windowClass.lpszClassName,
                                  "Handmade Hero",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  0,
                                  0,
                                  instance,
                                  0);
    if (window) {
      int x_offset = 0;
      int y_offset = 0;

      global_running = true;
      while (global_running) {
        MSG message;
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            global_running = false;
          }
          TranslateMessage(&message);
          DispatchMessage(&message);
        }
        RenderWeirdGradient(global_back_buffer, x_offset, y_offset);

        HDC deviceContext = GetDC(window);
        win32_window_dimension dimension = Win32GetWindowDimension(window);
        Win32DisplayWindow(deviceContext, global_back_buffer, dimension.width, dimension.height);
        ReleaseDC(window, deviceContext);

        x_offset++;
        y_offset += 2;
      }
    } else {
      // TODO: logging
    }
  } else {
    // TODO: logging
  }
  return 0;
}
