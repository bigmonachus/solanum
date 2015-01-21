@echo off
IF NOT EXIST third_party mkdir third_party
IF NOT EXIST third_party\imgui git clone https://github.com/ocornut/imgui.git third_party\imgui

pushd third_party\imgui
cl -c imgui.cpp
lib imgui.obj -OUT:imgui.lib
popd
