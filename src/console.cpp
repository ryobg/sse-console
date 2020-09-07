/**
 * @file console.cpp
 * @brief General utilities related to the SSE Console application
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
#include <gsl/gsl_util>
#include <iomanip>
#include <ctime>
#include <sstream>

//--------------------------------------------------------------------------------------------------

console_t console;

std::string const&
plugin_name ()
{
    static std::string v = "sse-console";
    return v;
}

//--------------------------------------------------------------------------------------------------

bool
setup ()
{
    extern void ImGuiInputTextCallbackData_DeleteChars (ImGuiInputTextCallbackData*, int, int);
    imgui.ImGuiInputTextCallbackData_DeleteChars = ImGuiInputTextCallbackData_DeleteChars;

    if (!load_settings ())
        return false;

    if (!setup_render ())
        return false;

    if (!load_help_file ()) // Not mandatory, but tells about broken setup.
        return false;

    load_log_file (plugin_directory () + "default.log");
    return true;
}

//--------------------------------------------------------------------------------------------------

void
record_log_message (bool outgoing, std::string const& msg)
{
    std::stringstream ss;

    auto now_c = std::time (nullptr);
    auto loc_c = std::localtime (&now_c);
    ss << std::put_time (loc_c, "[%Y-%m-%d %H:%M:%S]");

    if (outgoing)
        ss << ++console.counter_out << '>';
    else
        ss << ++console.counter_in  << '<';
    ss << ' ';

    log_index ndx;
    ndx.out = outgoing;
    ndx.begin = static_cast<std::uint32_t> (console.log_data.size ());
    ndx.mid = std::uint32_t (ss.str ().size ());

    ss << trimmed_both (msg, ' ');
    auto str = ss.str ();
    ndx.end = std::uint32_t (str.size ());

    console.log_indexes.push_back (ndx);
    console.log_data.insert (console.log_data.end (), str.begin (), str.end ());
}

//--------------------------------------------------------------------------------------------------

/// Bugged function in mainstream
/// @see https://github.com/ocornut/imgui/issues/3454

void
ImGuiInputTextCallbackData_DeleteChars (ImGuiInputTextCallbackData* self, int pos, int bytes_count)
{
    Expects (pos + bytes_count <= self->BufTextLen);
    char* dst = self->Buf + pos;
    const char* src = self->Buf + pos + bytes_count;
    while (char c = *src++)
        *dst++ = c;
    *dst = '\0';

    if (self->CursorPos >= pos + bytes_count)
        self->CursorPos -= bytes_count;
    else if (self->CursorPos >= pos)
        self->CursorPos = pos;
    self->SelectionStart = self->SelectionEnd = self->CursorPos;
    self->BufDirty = true;
    self->BufTextLen -= bytes_count;
}

//--------------------------------------------------------------------------------------------------

