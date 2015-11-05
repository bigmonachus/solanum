@echo off

IF NOT EXIST build mkdir build

pushd imgui
cl /wd4577 -c imgui.cpp
lib imgui.obj -OUT:imgui.lib
popd

:: 4820 is padding warning.
set common_c_flags=/nologo /GR- /EHa- /MT /WX /Wall /wd4820 /wd4514 /wd4505 /wd4201 /wd4100 /wd4189 /wd4577 /wd4710 /D_CRT_SECURE_NO_WARNINGS /FC /MP

set common_link_flags=user32.lib gdi32.lib  OpenGL32.lib ..\imgui\imgui.lib ..\third_party\glew32s.lib

pushd build
cl %common_c_flags% /Od /Oi /Zi /I..\third_party ..\src\win_solanum.cc /link %common_link_flags%
popd
