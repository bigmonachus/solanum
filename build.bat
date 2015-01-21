@echo off

IF NOT EXIST build mkdir build

set common_c_flags=/nologo /Gm- /GR- /EHa- /MT /WX /Wall /wd4514 /wd4505 /wd4201 /wd4100 /wd4189 /D_CRT_SECURE_NO_WARNINGS /FC /MP /FS

set common_link_flags=user32.lib gdi32.lib  OpenGL32.lib

pushd build
cl %common_c_flags% /Od /Oi /Zi ..\src\win_solanum.cc /link %common_link_flags%
popd
