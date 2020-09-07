/**
 * @file render.cpp
 * @brief Anything related to the GUI rendering.
 * @internal
 *
 * This file is part of Skyrim SE Console mod.
 *
 *   Console is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Console is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Console. If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Core
 *
 * @details
 */

#include "console.hpp"
#include <utils/misc.hpp>
#include <utils/winutils.hpp>
#include <string_view>

//--------------------------------------------------------------------------------------------------

static std::vector<char> input_text_buffer;
static std::vector<char> filter_text_buffer;
static std::vector<char> filter_help_buffer;

static int current_history;

static std::vector<log_index> const* filtered_log;
static records_filter<log_index> log_filter;

static std::vector<help_index> const* filtered_help;
static records_filter<help_index> help_filter;

static render_load_files render_load_log;
static render_load_files render_load_run;

static bool scroll_to_bottom;
static bool show_help;
static bool show_settings;
static bool show_save_log;
static bool log_to_clipboard;
static ImVec2 button_size;  ///< Public to keep consistency across windows

/// Current HWND, used for timer management
static HWND top_window = nullptr;

//--------------------------------------------------------------------------------------------------

bool
setup_render ()
{
    top_window = (HWND) imgui.igGetIO ()->ImeWindowHandle;
    input_text_buffer.clear ();
    input_text_buffer.resize (1024, '\0');
    filter_text_buffer.clear ();
    filter_text_buffer.resize (256, '\0');
    filter_help_buffer.clear ();
    filter_help_buffer.resize (256, '\0');
    current_history = 0;
    filtered_log = nullptr;
    log_filter.init (&console.log_data, &console.log_indexes, { 3, 4, 6 });
    help_filter.init (&console.help_data, &console.help_indexes, { 3, 4, 6 });
    render_load_log.init ("SSE Console: Load", {".log"});
    render_load_run.init ("SSE Console: Run", {".log", ".txt"});
    show_settings = false;
    show_save_log = false;
    button_size = ImVec2 {0, 0};
    log_to_clipboard = false;
    show_help = false;
    scroll_to_bottom = false;
    return true;
}

//--------------------------------------------------------------------------------------------------

static int
input_text_callback (ImGuiInputTextCallbackData* data)
{
    switch (data->EventFlag)
    {

    case ImGuiInputTextFlags_CallbackCompletion:
    {
        static std::vector<const char*> matches;
        static std::hash<std::string_view> hash;
        static std::size_t prev_uid = hash (std::string_view ("", 0));
        static int prev_start = 0, prev_len = 0;

        auto update_text = [&] (int start, int len, const char* newval)
        {
            imgui.ImGuiInputTextCallbackData_DeleteChars (data, start, len);
            imgui.ImGuiInputTextCallbackData_InsertChars (data, data->CursorPos, newval, nullptr);
            imgui.ImGuiInputTextCallbackData_InsertChars (data, data->CursorPos, " ", nullptr);
            prev_start = start, prev_len = std::strlen (newval) + 1;
            prev_uid = hash (std::string_view (data->Buf, data->BufTextLen));
        };

        // Allows scrolling through different matches
        auto curr_uid = hash (std::string_view (data->Buf, data->BufTextLen));
        if (prev_uid == curr_uid && matches.size () > 1)
        {
            std::rotate (matches.begin (), matches.begin () + 1, matches.end ());
            update_text (prev_start, prev_len, matches.front ());
            return 0;
        }

        // Find the start & end of the word
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_begin = word_end;
        for (; word_end < data->Buf + data->BufTextLen; ++word_end)
            if (*word_end == ' ')
                break;
        for (; word_begin > data->Buf; --word_begin)
            if (word_begin[-1] == ' ')
                break;

        // Small non-zero text is ignored as autocompletion - no reason
        matches.clear ();
        auto uprefix = uppercase_string (std::string (word_begin, word_end));
        if (uprefix.size () < 2)
            return 0;

        // Find matches
        for (auto i = console.completers.cbegin (); i != console.completers.cend (); ++i)
            if (uppercase_string (*i).rfind (uprefix, 0) == 0)
                matches.push_back (i->c_str ()); // Not expecting the completers to change at all

        if (!matches.empty ())
            update_text (int (word_begin - data->Buf), int (uprefix.size ()), matches.front ());
    }
        break;

    // Should updating the input mid-history browsing reset the story pointer back to the end?
    case ImGuiInputTextFlags_CallbackHistory:

        if (console.log_indexes.empty ())
            return 0;

        const char* left = nullptr;
        const char* mid = nullptr;
        const char* right = nullptr;
        static std::string prev_story;

        auto navigate = [&] (int step)
        {
            int i = current_history;
            int n = console.log_indexes.size () - 1;
            i = std::clamp (i+step, 0, n);
            for (; i >= 0 && i <= n; i += step)
                if (console.log_indexes[i].out)
                {
                    std::tie (left, mid, right) = extract_message (
                            console.log_data, console.log_indexes[i]);

                    // Ignore equal adjacent pairs
                    if (prev_story.size () != std::size_t (right - mid)
                            || !std::equal (mid, right, prev_story.cbegin ()))
                    {
                        prev_story.assign (mid, right);
                        current_history = i;
                        return;
                    }
                }
            // Stick to the current valid choice, if nothing earlier/later was found
            if (current_history >= 0 && current_history < int (console.log_indexes.size ())
                    && console.log_indexes[current_history].out)
            {
                std::tie (left, mid, right) = extract_message (
                        console.log_data, console.log_indexes[current_history]);
            }
        };

        if (data->EventKey == ImGuiKey_UpArrow)
            navigate (-1);
        else if (data->EventKey == ImGuiKey_DownArrow)
            navigate (+1);

        if (!mid)
            return 0;

        imgui.ImGuiInputTextCallbackData_DeleteChars (data, 0, data->BufTextLen);
        imgui.ImGuiInputTextCallbackData_InsertChars (data, 0, mid, right);
        break;
    };
    return 0;
}

//--------------------------------------------------------------------------------------------------

static int
filter_text_callback (ImGuiInputTextCallbackData* data) { return 0; }

//--------------------------------------------------------------------------------------------------

static void
render_save_log ()
{
    static std::vector<char> name (128);

    if (imgui.igBegin ("SSE Console: Save as", &show_save_log, 0))
    {
        imgui.igText (plugin_directory ().c_str ());
        imgui.igInputText (".log", name.data (), int (name.size ()), 0, nullptr, nullptr);
        if (imgui.igButton ("Cancel", button_size))
            show_save_log = false;
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Save", button_size))
        {
            auto base = trimmed_both (name.data (), " ");
            if (base.size ())
            {
                save_log_file (plugin_directory () + base + ".log");
                show_save_log = false;
            }
        }
    }
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

static void
render_settings ()
{
    if (imgui.igBegin ("SSE Console: Settings", &show_settings, 0))
    {
        render_font_settings (console.gui_font, false);

        imgui.igText ("");
        render_font_settings (console.log_font, false);

        imgui.igText ("");
        imgui.igText ("Log colors:");
        render_color_setting ("Prompt##Log color", console.prompt_color);
        render_color_setting ("Commands##Log color", console.out_color);
        render_color_setting ("Feedback##Log color", console.in_color);

        imgui.igText ("");
        imgui.igText ("Help colors:");
        render_color_setting ("Names##Help color", console.help_names_color);
        render_color_setting ("Parameters##Help color", console.help_params_color);
        render_color_setting ("Brief text##Help color", console.help_brief_color);
        render_color_setting ("Details##Help color", console.help_details_color);

        imgui.igText ("");
        imgui.igText ("Running scripts:");
        if (imgui.igDragInt ("Delay", &console.execution_delay, 1.f,
                std::max (50, USER_TIMER_MINIMUM), std::min (60'000, USER_TIMER_MAXIMUM),
                "%d milliseconds"))
        {
            extern void update_timer (int period);
            if (!console.commands.empty ())
                update_timer (console.execution_delay);
        }

        imgui.igText ("");
        if (imgui.igButton ("Save", button_size))
            save_settings ();
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Load", button_size))
            load_settings ();
    }
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

static void
render_help ()
{
    if (imgui.igBegin ("SSE Console: Help", &show_help, 0))
    {
        imgui.igTextUnformatted (" Filter:", nullptr);
        imgui.igSameLine (0, -1);
        imgui.igSetNextItemWidth (-1);
        if (imgui.igInputText ("##Filter",
                    filter_help_buffer.data (), int (filter_help_buffer.size ()),
                    0, &filter_text_callback, nullptr))
        {
            filtered_help = help_filter.update (filter_help_buffer.data ());
        }

        imgui.igBeginChild ("##Help", ImVec2 {0, 0}, false, 0);
        auto const* display_records = filtered_help ? filtered_help : &console.help_indexes;
        auto clipper = imgui.ImGuiListClipper_ImGuiListClipper (int (display_records->size ()), -1);
        while (imgui.ImGuiListClipper_Step (clipper))
            for (int i = clipper->DisplayStart; i < clipper->DisplayEnd; ++i)
        {
            auto [names, params, brief, details, end] =
                extract_message (console.help_data, (*display_records)[i]);

            imgui.igPushTextWrapPos (0.f);
            imgui.igText ("");
            int pops = 0;
            imgui.igPushStyleColorU32 (ImGuiCol_Text, console.help_names_color); ++pops;
            imgui.igTextUnformatted (names, params);
            if (params != brief)
            {
                imgui.igPushStyleColorU32 (ImGuiCol_Text, console.help_params_color); ++pops;
                imgui.igTextUnformatted (params, brief);
            }
            imgui.igPushStyleColorU32 (ImGuiCol_Text, console.help_brief_color); ++pops;
            imgui.igTextUnformatted (brief, details);
            imgui.igPushStyleColorU32 (ImGuiCol_Text, console.help_details_color); ++pops;
            imgui.igTextUnformatted (details, end);
            imgui.igPopStyleColor (pops);
            imgui.igPopTextWrapPos ();
        }
        imgui.ImGuiListClipper_destroy (clipper);
        imgui.igEndChild ();
    }
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

static void
render_log ()
{
    float footer_height = 3 * imgui.igGetFrameHeightWithSpacing ();
    imgui.igPushFont (console.log_font.imfont);
    imgui.igBeginChild ("##Log", ImVec2 { 0, -footer_height }, false, 0);

    if (log_to_clipboard)
        imgui.igLogToClipboard (-1);

    // TODO: The ImGuiListClipper works only for evenly spaced items or the igSetScrollHereY
    // down will botch on each multiline record, making everything to look awkward. Hence, to
    // get proper position a full blown rendering is done, which botches the FPS in turn. For
    // 10k records here, it drains near 6 fps. To circumvent this a custom clipping must be
    // done, which likely will involve pre-computing the height of each item as function of the
    // text font, scale, word wrapping due to widget size/resize, number of newlines in the
    // record and item spacing. Another likely solution is to find which change and expose of the
    // ImGui internals will sove the problem during the SetScrollHereY. Usage of the log file
    // saving/loading features are suggested to workaround the fps loss.
    auto const* display_records = filtered_log ? filtered_log : &console.log_indexes;
    for (std::size_t i = 0, n = display_records->size (); i < n; ++i)
    {
        auto ndx = (*display_records)[i];
        auto [left, mid, right] = extract_message (console.log_data, ndx);

        imgui.igPushStyleColorU32 (ImGuiCol_Text, console.prompt_color);
        imgui.igTextUnformatted (left, mid);
        imgui.igPopStyleColor (1);

        imgui.igSameLine (0, -1);
        imgui.igPushTextWrapPos (0.f);
        imgui.igPushStyleColorU32 (ImGuiCol_Text, ndx.out ? console.out_color:console.in_color);
        imgui.igTextUnformatted (mid, right);
        imgui.igPopStyleColor (1);
        imgui.igPopTextWrapPos ();
    }

    if (log_to_clipboard)
    {
        log_to_clipboard = false;
        imgui.igLogFinish ();
    }

    if (scroll_to_bottom)
    {
        scroll_to_bottom = false;
        imgui.igSetScrollHereY (1.f);
    }

    imgui.igEndChild ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

static void
execute_command (std::string cmd)
{
    trim_both (cmd, ' ');
    if (cmd.empty ())
        return;

    record_log_message (true, cmd);
    skyrim_log::last_message ("");
    skyrim_console::execute (cmd);
    record_log_message (false, skyrim_log::last_message ());
    current_history = console.log_indexes.size ();
    filtered_log = log_filter.update (filter_text_buffer.data (), true);
    scroll_to_bottom = true;
}

//--------------------------------------------------------------------------------------------------

static VOID CALLBACK
timer_callback (HWND hwnd, UINT message, UINT_PTR idTimer, DWORD dwTime)
{
    if (console.commands.empty ())
    {
        extern void update_timer (int);
        update_timer (0);
        return;
    }

    execute_command (std::move (console.commands.back ()));
    console.commands.pop_back ();
}

//--------------------------------------------------------------------------------------------------

void
update_timer (int period)
{
    if (period <= 0)
        ::KillTimer (top_window, (UINT_PTR) timer_callback);

    else if (!::SetTimer (top_window, (UINT_PTR) timer_callback, UINT (period), timer_callback))
    {
        log () << "Failed to create timer: "
            << format_utf8message (::GetLastError ()) << std::endl;
    }
}

//--------------------------------------------------------------------------------------------------

void render (int active)
{
    static bool old_active = active;
    bool reclaim_input = std::exchange (old_active, active) != active;
    if (!active)
        return;

    default_theme theme_on;
    imgui.igPushFont (console.gui_font.imfont);

    imgui.igSetNextWindowSize (ImVec2 { 800, 600 }, ImGuiCond_FirstUseEver);
    if (imgui.igBegin ("SSE Console", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
    {
        render_log ();

        imgui.igSetNextItemWidth (-1);
        if (imgui.igInputText ("##Input", input_text_buffer.data (), int(input_text_buffer.size ()),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                ImGuiInputTextFlags_CallbackHistory, &input_text_callback, nullptr))
        {
            execute_command (input_text_buffer.data ());
            input_text_buffer[0] = 0;
            reclaim_input = true;
        }
        imgui.igSetItemDefaultFocus ();
        if (reclaim_input)
            imgui.igSetKeyboardFocusHere (-1);

        // New line
        button_size.x = imgui.igCalcTextSize (" Load & Run ", nullptr, false, -1.f).x;
        render_load_log.button_size = render_load_run.button_size = button_size;

        if (imgui.igButton ("Run", button_size))
            render_load_run.queue_render ();

        imgui.igSameLine (0, -1);
        imgui.igButton ("Log", button_size);
        if (imgui.igBeginPopupContextItem ("##Log popup", 0))
        {
            if (imgui.igButton ("Copy##Log popup", button_size))
            {
                log_to_clipboard = true;
                imgui.igCloseCurrentPopup ();
            }
            if (imgui.igButton ("Save##Log popup", button_size))
            {
                show_save_log = true;
                imgui.igCloseCurrentPopup ();
            }
            if (imgui.igButton ("Load##Log popup", button_size))
            {
                render_load_log.queue_render ();
                imgui.igCloseCurrentPopup ();
            }
            if (imgui.igButton ("Clear##Log popup", button_size))
            {
                log_filter.clear ();
                console.log_data.clear ();
                console.log_indexes.clear ();
                console.counter_in = console.counter_out = 0;
                filtered_log = nullptr;
                current_history = 0;
                imgui.igCloseCurrentPopup ();
            }
            imgui.igEndPopup ();
        }

        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Settings", button_size))
            show_settings = true;

        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Help", button_size))
            show_help = true;

        imgui.igSameLine (0, -1);
        imgui.igTextUnformatted (" Filter:", nullptr);

        imgui.igSameLine (0, -1);
        imgui.igSetNextItemWidth (-1);
        if (imgui.igInputText ("##Filter",
                    filter_text_buffer.data (), int (filter_text_buffer.size ()),
                    0, &filter_text_callback, nullptr))
        {
            filtered_log = log_filter.update (filter_text_buffer.data ());
        }

        // New line
        imgui.igTextDisabled ("Selected: %X  ", skyrim_console::selected_form ());

        imgui.igSameLine (0, -1);
        imgui.igTextDisabled ("FPS: %.1f", imgui.igGetIO ()->Framerate);
    }
    imgui.igEnd ();

    if (auto f = render_load_log.update (); !f.empty ())
        if (load_log_file (f))
        {
            filtered_log = nullptr;
            current_history = 0;
            log_filter.clear ();
            filtered_log = log_filter.update (filter_text_buffer.data (), true);
        }

    if (auto f = render_load_run.update (); !f.empty ())
    {
        if (load_run_file (f) && !console.commands.empty ())
            update_timer (console.execution_delay);
    }

    if (show_save_log)
        render_save_log ();
    if (show_settings)
        render_settings ();
    if (show_help)
        render_help ();

    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

