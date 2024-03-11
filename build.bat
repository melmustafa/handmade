@REM TODO: change this to a make file

@ECHO OFF

IF NOT EXIST .\build\win32 mkdir .\build\win32
IF NOT EXIST .\bin\win32 mkdir .\bin\win32
g++ -shared -fno-exceptions -D HANDMADE_SLOW=1 -D HANDMADE_INTERNAL=1 -Wall -Og -O0 -fopenmp code\handmade.cpp -o bin\win32\handmade.dll -g
g++ -fno-exceptions -D HANDMADE_SLOW=1 -D HANDMADE_INTERNAL=1 -Wall -Og -O0 -fopenmp code\win32_handmade.cpp -o build\win32\win32_handmade.exe -g -lgdi32 -lwinmm