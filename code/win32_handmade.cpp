#include <windows.h>

LRESULT MainWindowCallback(HWND window,
                           UINT message,
                           WPARAM wParam,
                           LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
    case WM_SIZE: {
      OutputDebugString("WM_SIZE\n");
    } break;
    case WM_DESTROY: {
      OutputDebugString("WM_DESTROY\n");
    } break;
    case WM_CLOSE: {
      OutputDebugString("WM_CLOSE\n");
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
      static DWORD operation = WHITENESS;
      if (operation == WHITENESS) {
        operation = BLACKNESS;
      } else {
        operation = WHITENESS;
      }
      PatBlt(deviceContext, x, y, width, height, operation);

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
    HWND windowHandle = CreateWindowEx(
        0, windowClass.lpszClassName, "Handmade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

    if (windowHandle != NULL) {
      MSG message;
      for (;;) {
        BOOL messageResult = GetMessage(&message, 0, 0, 0);
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
