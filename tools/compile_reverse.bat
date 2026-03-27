@echo off
set VCPKG=E:\prog\BBFx-Revival\bbfx-revival\build\windows-debug\vcpkg_installed\x64-windows
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

cd /d E:\prog\BBFx-Revival\bbfx-revival\tools
cl.exe /EHsc /std:c++17 /I"%VCPKG%\include" theora_reverse.cpp /Fe:theora_reverse.exe /link /LIBPATH:"%VCPKG%\lib" theora.lib theoradec.lib theoraenc.lib ogg.lib
