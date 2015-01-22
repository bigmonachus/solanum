#pragma once

struct TimerState
{
    int window_width;
    int window_height;
};

static void timer_step_and_render(TimerState* state)
{
    ImVec2 size = {(float)state->window_width, (float)state->window_height};
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.5, 0.5, 0.5, 0.5});
    ImGui::Begin("Main", NULL, size);
    ImGui::Text("Solanum. A timer to get shit done.");
    char buffer[256];
    sprintf_s(buffer, 256, "Hello there timer: %d\n", 0);
    ImGui::Text(buffer);
    static bool scary = false;
    if (ImGui::Button("Click me"))
    {
        scary ^= 1;
    }
    if (scary)
    {
        ImGui::Text("BOO!");
    }
    ImGui::End();
    ImGui::Render();
}
