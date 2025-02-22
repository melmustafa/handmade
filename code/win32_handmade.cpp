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
global_variable bool running;

global_variable BITMAPINFO bitmapInfo;
global_variable void* bitmapMemory;
global_variable int bitmapWidth;
global_variable int bitmapHeight;
global_variable int bytesPerPixel = 4;

internal void RenderWeirdGradient(int xOffset, int yOffset) {
  int width = bitmapWidth;
  int height = bitmapHeight;

  int pitch = bytesPerPixel * bitmapWidth;
  uint8* row = (uint8*)bitmapMemory;
  for (int y = 0; y < height; ++y) {
    uint32* pixel = (uint32*)row;
    for (int x = 0; x < width; ++x) {
      *pixel =
          ((x * 255 / width) + xOffset) | (((y * 255 / height) + yOffset) << 8);

      pixel++;
    }
    row += pitch;
  }
}

internal void Win32ResizeDIBSection(int width, int height) {
  if (bitmapMemory) {
    VirtualFree(bitmapMemory, 0, MEM_RELEASE);
  }

  bitmapWidth = width;
  bitmapHeight = height;

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = width;
  bitmapInfo.bmiHeader.biHeight = -height;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = (width * height) * bytesPerPixel;
  bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32UpdateWindow(HDC deviceContext,
                                RECT* clientRect,
                                int x,
                                int y,
                                int width,
                                int height) {
  int windowWidth = clientRect->right - clientRect->left;
  int windowHeight = clientRect->bottom - clientRect->top;
  StretchDIBits(deviceContext,
                0,
                0,
                bitmapWidth,
                bitmapHeight,
                0,
                0,
                windowWidth,
                windowHeight,
                bitmapMemory,
                &bitmapInfo,
                DIB_RGB_COLORS,
                SRCCOPY);
}

LRESULT MainWindowCallback(HWND window,
                           UINT message,
                           WPARAM wParam,
                           LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
    case WM_DESTROY: {
      // TODO: this might be an error
      running = false;
    } break;
    case WM_CLOSE: {
      // TODO: handle with an interaction with the message
      running = false;
    } break;
    case WM_SIZE: {
      // TODO: casey is using client rect check the performance pelnty for this
      // RECT clientRect;
      // GetClientRect(window, &clientRect);

      int width = (short)lParam;
      int height = (short)(lParam >> 16);
      Win32ResizeDIBSection(width, height);
      OutputDebugString("WM_SIZE\n");
    } break;
    case WM_ACTIVATE: {
      OutputDebugString("WM_ACTIVATE\n");
    } break;
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC deviceContext = BeginPaint(window, &paint);
      int x = paint.rcPaint.left;
      int y = paint.rcPaint.top;
      int height = paint.rcPaint.bottom - paint.rcPaint.top;
      int width = paint.rcPaint.right - paint.rcPaint.left;

      RECT clientRect;
      GetClientRect(window, &clientRect);

      RenderWeirdGradient(0, 0);

      Win32UpdateWindow(deviceContext, &clientRect, x, y, height, width);
      EndPaint(window, &paint);
    } break;
    default: {
      OutputDebugString("default\n");
      result = DefWindowProc(window, message, wParam, lParam);
    } break;
  }

  return result;
}

int WinMain(HINSTANCE instance,
            HINSTANCE prevInstance,
            LPSTR cmdLine,
            int showCmd) {
  WNDCLASS windowClass = {};
  // TODO: check if those are still needed
  windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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
      int xOffset = 0;
      int yOffset = 0;

      running = true;
      while (running) {
        MSG message;
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            running = false;
          }

          TranslateMessage(&message);
          DispatchMessage(&message);
        }
        RenderWeirdGradient(xOffset, yOffset);

        HDC deviceContext = GetDC(window);
        RECT clientRect;
        GetClientRect(window, &clientRect);
        int height = clientRect.bottom - clientRect.top;
        int width = clientRect.right - clientRect.left;
        Win32UpdateWindow(deviceContext, &clientRect, 0, 0, width, height);
        ReleaseDC(window, deviceContext);

        xOffset++;
      }
    } else {
      // TODO: logging
    }
  } else {  // TODO: logging
  }
  return 0;
}
