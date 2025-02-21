#include <windows.h>

#define internal static
#define global_variable static
#define local_persist static

// NOTE: this is a local (only works in this file) global variable for now
global_variable bool running;
global_variable BITMAPINFO bitmapInfo;
global_variable void* bitmapMemory;
global_variable HBITMAP bitmapHandle;
global_variable HDC bitmapDeviceContext;

internal void Win32ResizeDIBSection(int width, int height) {
  if (bitmapHandle) {
    DeleteObject(bitmapHandle);
  }

  if (!bitmapDeviceContext) {
    bitmapDeviceContext = CreateCompatibleDC(0);
  }

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = width;
  bitmapInfo.bmiHeader.biHeight = height;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  // TODO: check this out again to check for allocation problems

  bitmapHandle = CreateDIBSection(
      bitmapDeviceContext, &bitmapInfo, DIB_RGB_COLORS, &bitmapMemory, NULL, 0);
}

internal void Win32UpdateWindow(HDC deviceContext,
                                int x,
                                int y,
                                int width,
                                int height) {
  /*StretchDIBits(deviceContext,*/
  /*              x,*/
  /*              y,*/
  /*              width,*/
  /*              height,*/
  /*              x,*/
  /*              y,*/
  /*              width,*/
  /*              height,*/
  /*              [in] const VOID* lpBits,*/
  /*              [in] const BITMAPINFO* lpbmi,*/
  /*              DIB_RGB_COLORS,*/
  /*              SRCCOPY);*/
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
      Win32UpdateWindow(deviceContext, x, y, height, width);
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
    HWND windowHandle = CreateWindowExA(0,
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

    if (windowHandle != NULL) {
      running = true;
      MSG message;
      while (running) {
        BOOL messageResult = GetMessageA(&message, 0, 0, 0);
        if (messageResult > 0) {
          TranslateMessage(&message);
          DispatchMessage(&message);
        } else {
          // TODO: logging
          break;
        }
      }
    } else {
      // TODO: logging
    }
  } else {  // TODO: logging
  }
  return 0;
}
