#pragma once

#include <time.h>

typedef int32_t bool32;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

struct TimeRecord
{
    int64 timestamp;
    int64 elapsed;
};

struct TimerState
{
    int window_width; // This refers to the main imgui window
    int window_height;

    TimeRecord* records;
    size_t records_size;
    int64 num_records;

    int64 time_unit_in_s;
    int64 num_seconds;

    time_t begin_time;
    bool32 started;
    bool32 is_long;
};

static void timer_step_and_render(TimerState* state)
{
    char buffer[256];
    time_t current_time;
    time(&current_time);

    ImVec2 size = {(float)state->window_width, (float)state->window_height};
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.17f, 0.23f, 0.42f, 0.5});

    ImGui::Begin("", NULL, size);
    ImGui::SetWindowPos(ImVec2(0,0));
    ImGui::Text("Solanum. A timer to get shit done.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!state->started)
    {
        int64 hours = state->num_seconds / (60 * 60);
        int64 minutes = state->num_seconds / 60;
        int64 seconds = state->num_seconds % 60;
        snprintf(buffer, 256, "Time logged in the past 24 hours: %uh %um %us", hours, minutes, seconds);
        ImGui::Text(buffer);
        ImGui::Spacing();
        state->is_long = false;
        if (ImGui::Button("Begin (short)"))
        {
            state->started = true;
            time_t begin_time;
            time(&begin_time);
            state->begin_time = begin_time;
        }
        ImGui::SameLine(0, 30);
        if (ImGui::Button("Begin (long)"))
        {
            state->started = true;
            time_t begin_time;
            time(&begin_time);
            state->begin_time = begin_time;
            state->is_long = true;
        }
        ImGui::SetCursorPosY((float)state->window_height - 40);
        if (ImGui::Button("Quit"))
        {
            platform_quit();
        }
    }

    if (state->started)
    {
        int64 elapsed = current_time - state->begin_time;
        int factor = state->is_long? 2 : 1;
        int64 time_left = (state->time_unit_in_s * factor) - elapsed;

        int64 hours = elapsed / (60 * 60);
        int64 minutes = elapsed / 60;
        int64 seconds = elapsed % 60;
        snprintf(buffer, 256, "Time elapsed: %uh %um %us", hours, minutes, seconds);

        ImGui::Text(buffer);
        bool32 stopping = ImGui::Button("Stop");
        bool32 alert_user = false;
        if (time_left <= 0)
        {
            stopping = true;
            alert_user = true;
        }

        if (stopping)
        {
            state->num_seconds += elapsed;
            state->started = false;
            TimeRecord record = {};
            record.timestamp = current_time;
            record.elapsed = elapsed;
            state->records[state->num_records++] = record;
            if (alert_user)
            {
                platform_alert();
            }
            platform_save_state(state);
        }
    }

    ImGui::End();
    ImGui::Render();
}
