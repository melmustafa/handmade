@echo off

mkdir build
pushd build
g++ ../code/win32_handmade.cpp -o handmade -lgdi32
popd
