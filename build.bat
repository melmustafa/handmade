@echo off

mkdir build
pushd build
g++ ../code/win32_handmade.cpp -g -o handmade -lgdi32
popd
