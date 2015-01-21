@echo off

IF NOT EXIST build mkdir build

set common_c_flags=/nologo /GR- /EHa- /MT /WX /Wall /wd4514 /wd4505 /wd4201 /wd4100 /wd4189 /D_CRT_SECURE_NO_WARNINGS /FC /MP

set common_link_flags=user32.lib gdi32.lib  OpenGL32.lib ..\third_party\imgui\imgui.lib ..\third_party\glew32s.lib

pushd build
cl %common_c_flags% /Od /Oi /Zi /I..\third_party ..\src\win_solanum.cc /link %common_link_flags%
popd
