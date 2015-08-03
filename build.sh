if [ ! -d third_party/imgui ]; then
  git clone https://github.com/ocornut/imgui.git third_party/imgui
fi

cd third_party/imgui
clang++ imgui.cpp -g -c -o imgui.o
ar rcs imgui.a imgui.o
cd ../..

clang++ \
  -O0 -g \
  -std=c++11 \
  -Wno-c++11-compat-deprecated-writable-strings \
  -Wno-writable-strings \
  -I./third_party/ \
  src/sdl_solanum.cc \
  third_party/imgui/imgui.a \
  -lGL -lSDL2 \
  -o solanum

