@echo off

IF NOT EXIST build mkdir build

pushd imgui
cl /wd4577 -c /Zi imgui.cpp imgui_draw.cpp
lib imgui.obj imgui_draw.obj -OUT:imgui.lib
popd

:: 4820 is padding warning.
set common_c_flags=/nologo /GR- /EHa- /MT /WX /Wall /wd4820 /wd4800 /wd4514 /wd4505 /wd4201 /wd4100 /wd4189 /wd4577 /wd4710 /wd4668 /D_CRT_SECURE_NO_WARNINGS /FC /MP

set common_link_flags=user32.lib gdi32.lib  OpenGL32.lib ..\imgui\imgui.lib ..\third_party\glew32s.lib

pushd build
::cl %common_c_flags% /Od /Oi /Zi /I..\imgui /I..\third_party ..\src\win_solanum.cc /link %common_link_flags%
cl %common_c_flags% /Od /Oi /Zi /I..\imgui /I..\third_party /I..\SDL2-2.0.3\include ..\src\sdl_solanum.cc ..\src\imgui_impl_sdl_gl3.cpp /link %common_link_flags% ..\SDL2-2.0.3\lib\x64\SDL2.lib
popd
