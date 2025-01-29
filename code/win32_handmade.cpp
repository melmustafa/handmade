#include <windows.h>

int WinMain(HINSTANCE instance,
            HINSTANCE prevInstance,
            LPSTR cmdLine,
            int showCmd) {
  MessageBox(0, "This is handmade hero.", "Handmade Hero",
             MB_OK | MB_ICONINFORMATION);
  return 0;
}
