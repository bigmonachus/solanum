#pragma once

#include <time.h>

typedef int32_t bool32;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;


enum
{
    SOLANUM_NONE = 0,
    SOLANUM_QUIT = (1 << 1),
};

int g_solanum_message_queue = SOLANUM_NONE;

void solanum_post_exit()
{
    g_solanum_message_queue |= SOLANUM_QUIT;
    platform_quit();
}

#pragma pack(push)
#pragma pack(1)
struct TimeRecord
{
    int16 elapsed;
    int64 timestamp;
};
#pragma pack(pop)

enum TimerType
{
    TimerType_SHORT,
    TimerType_LONG,
    TimerType_ETERNAL
};

struct TimerState
{
    int window_width; // This refers to the main imgui window
    int window_height;

    TimeRecord* records;
    size_t records_size;
    int64 num_records;

    int16 time_unit_in_s;
    int num_seconds;

    time_t begin_time;
    bool32 started;
    TimerType timer_type;

    time_t pause_time;
    int16 pause_elapsed;
    bool32 paused;

    bool32 editing_last_entry;

    int64 time_persp;  // Point of reference for timer quick report
    char* curr_phrase;
    char* prev_phrase;
};

static char* phrases[] =
{
    "Get some shit done!",
    "You should be coding.",
    "\"80%% of life is showing up\" -- Woody Allen",
};

#define TEXT_BUFFER_SIZE 256
static void format_seconds(char* buffer, char* msg, int in_seconds)
{
    int hours = in_seconds / (60 * 60);
    int minutes = (in_seconds / 60) % 60;
    int seconds = in_seconds % 60;
    snprintf(buffer, TEXT_BUFFER_SIZE, "%s: %dh %dm %ds", msg, hours, minutes, seconds);
}

static void timer_step_and_render(TimerState* state)
{
    char buffer[TEXT_BUFFER_SIZE];
    time_t current_time;
    time(&current_time);

    // Old blue color
    int style_stack = 0;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.23f, 0.23f, 0.23f, 1.0f}); ++style_stack;

    bool show_solanum = true;
    ImGui::Begin("Solanum", &show_solanum);
    ImGui::SetWindowSize({400,200});

    if (!state->curr_phrase) {
        int num_phrases = sizeof(phrases) / sizeof(char*);
        int maxloops = 100;
        int i = 0;
        while (!state->prev_phrase || !state->curr_phrase || !strcmp(state->curr_phrase, state->prev_phrase)) {
            time(&current_time);
            state->curr_phrase = phrases[current_time % num_phrases];
            if (!state->prev_phrase || i++ >= maxloops) break;
        }
    }

    bool change_persp = false;

    if (state->editing_last_entry) {
        bool32 save = false;
        TimeRecord* record = &state->records[state->num_records-1];
        ImGui::Text("Seconds logged: %d", record->elapsed);
        static bool32 delete_open = false;
        int nv = record->elapsed;
        if ( ImGui::InputInt("10 min", &nv, 600) ) {
            if (nv < (1 << 16) && nv > 0) {
                record->elapsed = (int16)nv;
            }
        }
        format_seconds(buffer, "Entry", nv);
        ImGui::Text(buffer);
        if ( ImGui::Button("Delete") ) {
            delete_open = true;
        }
        if ( delete_open ) {
            ImGui::Text("Are you sure?");
            if ( ImGui::Button("Don't delete!") ) {
                state->editing_last_entry = false;
                delete_open = false;
                state->editing_last_entry = false;
            }
            if ( ImGui::Button("Yes, delete") ) {
                --state->num_records;
                save = true;
                change_persp = true;
                delete_open = false;
                state->editing_last_entry = false;
            }
        }
        if ( save ) {
            platform_save_state(state);
        }
        if( ImGui::Button("Finish") ) {
            state->editing_last_entry = false;
            change_persp = true;
        }
    } else if (!state->started) {
        ImGui::Text("Perspective: ");
        ImGui::SameLine();
        if (ImGui::Button("Now"))
        {
            change_persp = true;
            state->time_persp = current_time;
        }
        ImGui::SameLine();
        if (ImGui::Button("-1 hour"))
        {
            change_persp = true;
            state->time_persp -= 1 * 60 * 60;
        }
        ImGui::SameLine();
        if (ImGui::Button("-8 hours"))
        {
            change_persp = true;
            state->time_persp -= 8 * 60 * 60;
        }
        ImGui::SameLine();
        if (ImGui::Button("-24 hours"))
        {
            change_persp = true;
            state->time_persp -= 24 * 60 * 60;
        }
        ImGui::Separator();
        format_seconds(buffer, "Time logged", state->num_seconds);
        ImGui::Text(buffer);
        ImGui::Spacing();
        state->timer_type = TimerType_SHORT;
        if (ImGui::Button("Begin (short)"))
        {
            state->started = true;
            time_t begin_time;
            time(&begin_time);
            state->begin_time = begin_time;
            state->timer_type = TimerType_SHORT;
        }
        ImGui::SameLine(0, 20);
        if (ImGui::Button("Begin (long)"))
        {
            state->started = true;
            time_t begin_time;
            time(&begin_time);
            state->begin_time = begin_time;
            state->timer_type = TimerType_LONG;
        }
        ImGui::SameLine(0, 20);
        if (ImGui::Button("Begin (eternal)"))
        {
            state->started = true;
            time_t begin_time;
            time(&begin_time);
            state->begin_time = begin_time;
            state->timer_type = TimerType_ETERNAL;
        }

        ImGui::Separator();
        ImGui::Text(state->curr_phrase);

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 80);
        if (ImGui::Button("Quit"))
        {
            platform_quit();
        }
        ImGui::SameLine(0, 150);
        if (ImGui::Button("Edit last entry.")) {
            state->editing_last_entry = true;
        }

    } else if (state->started) {
        int16 elapsed = 0;
        {
            int64 elapsed_64 = current_time - state->begin_time - state->pause_elapsed;
            if (elapsed_64 <= (1 << 15) - 1)
            {
                elapsed = (int16) elapsed_64;
            }
            else
            {
                state->started = false;
            }
        }

        int16 time_unit_length = state->time_unit_in_s;
        switch(state->timer_type)
        {
        case TimerType_SHORT:
            {
                time_unit_length *= 1;
                break;
            }
        case TimerType_LONG:
            {
                time_unit_length *= 2;
                break;
            }
        case TimerType_ETERNAL:
            {
                time_unit_length = (int16)((1 << 15) - 1);
                break;
            }
        }
        int64 time_left = time_unit_length - elapsed;

        static time_t current_time_at_pause = 0;
        if (!state->paused)
        {
            format_seconds(buffer, "Time elapsed", elapsed);
            ImGui::Text(buffer);

            if (ImGui::Button("Pause"))
            {
                current_time_at_pause = current_time;
                state->pause_time = current_time - state->pause_elapsed;
                state->paused = true;
            }
        }
        else
        {
            state->pause_elapsed = (int16)(current_time - state->pause_time);
            if (ImGui::Button("Resume"))
            {
                state->paused = false;
            }

            format_seconds(buffer, "PAUSED for", (int)(current_time - current_time_at_pause));
            ImGui::Text(buffer);
        }

        ImGui::SameLine(0, 30);
        bool32 stopping = ImGui::Button("Stop");

        bool32 alert_user = false;
        if (time_left <= 0)
        {
            stopping = true;
            alert_user = true;
        }

        if (g_solanum_message_queue & SOLANUM_QUIT)
        {
            stopping = true;
        }


        if (stopping)
        {
            state->prev_phrase = state->curr_phrase;
            state->curr_phrase = NULL;
            state->started = false;
            state->pause_elapsed = 0;
            state->paused = false;
            TimeRecord record = {};
            record.timestamp = current_time;

            // This can happen if we leave a timer running and the computer goes to sleep.
            if (elapsed > time_unit_length)
            {
                elapsed = time_unit_length;
            }

            record.elapsed = elapsed;
            state->num_seconds += elapsed;
            state->records[state->num_records++] = record;
            if (alert_user)
            {
                platform_alert();
            }
            platform_save_state(state);
        }
    }
    if (change_persp) {
        state->num_seconds = 0;
        for (int i = 0; i < state->num_records; ++i) {
            if (state->records[i].timestamp >= state->time_persp) {
                state->num_seconds += state->records[i].elapsed;
            }
        }
    }



    ImGui::End();
    ImGui::PopStyleColor(style_stack);
    ImGui::Render();
}
