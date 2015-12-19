if [ ! -d build ]; then
    mkdir build
fi

cd imgui
clang++ imgui.cpp imgui_draw.cpp -g -c
ar rcs imgui.a imgui_draw.o imgui.o
cd ..

cd build

clang++ \
  -O0 -g \
  -std=c++11 \
  -Wno-c++11-compat-deprecated-writable-strings \
  `pkg-config --cflags glew` \
  -I../third_party/ -I../imgui \
  ../src/sdl_solanum.cc \
  ../src/imgui_impl_sdl_gl3.cpp \
  ../imgui/imgui.a \
  -lSDL2 \
  `pkg-config --libs glew` \
  -o solanum

cd ..

