/**
 * @file fileio.cpp
 * @brief File I/O ops area, as well default initialization of some stuff
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
#include <utils/winutils.hpp>
#include <charconv>

static const struct {
    std::filesystem::path
        settings = plugin_directory () + "settings.json",
        help_sse = plugin_directory () + "help_sse.json",
        help_gui = plugin_directory () + "help_gui.json",
        help_alias = plugin_directory () + "help_alias.json";
}
locations;

//--------------------------------------------------------------------------------------------------

bool
save_log_file (std::filesystem::path const& filename)
{
    try
    {
        std::ofstream fo (filename);
        if (!fo.is_open ())
        {
            log () << "Unable to open " << filename << " for writting." << std::endl;
            return false;
        }

        for (auto const& i: console.log_indexes)
        {
            auto [b, m, e] = extract_message (console.log_data, i);
            fo << std::string_view (b, std::size_t (e-b)) << std::endl;
        }
    }
    catch (std::exception const& ex)
    {
        log () << __func__ << ":" << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_log_file (std::filesystem::path const& filename)
{
    try
    {
        std::ifstream fi (filename);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << filename << " for reading." << std::endl;
            return false;
        }

        std::vector<char> log_data;
        std::vector<log_index> log_indexes;
        int counter_out = 0, counter_in = 0;
        int last_cnt[4] = {-1,-1,-1,-1};

        for (std::string row; std::getline (fi, row); )
        {
            row = trim_both (row, " \r\n");

            auto mid = row.find_first_of ("><");
            if (mid == std::string::npos)
                continue;

            log_index i;
            i.begin = log_data.size ();
            i.mid = std::min (mid + 2, row.size ());
            i.end = row.size ();
            i.out = row[mid] == '>';

            log_indexes.push_back (i);
            log_data.insert (log_data.end (), row.begin (), row.end ());

            if (i.out) last_cnt[0] = int (i.begin), last_cnt[1] = int (i.mid);
            else       last_cnt[2] = int (i.begin), last_cnt[3] = int (i.mid);
        }

        if (last_cnt[0] > 0)
        {
            std::string_view in (&log_data[last_cnt[0]], last_cnt[1]);
            in.remove_prefix (in.find (']') + 1);
            std::from_chars (in.data (), in.data () + in.size (), counter_out);
        }
        if (last_cnt[2] > 0)
        {
            std::string_view in (&log_data[last_cnt[2]], last_cnt[1]);
            in.remove_prefix (in.find (']') + 1);
            std::from_chars (in.data (), in.data () + in.size (), counter_in);
        }

        console.log_indexes.swap (log_indexes);
        console.log_data.swap (log_data);
        console.counter_in = counter_in;
        console.counter_out = counter_out;
    }
    catch (std::exception const& ex)
    {
        log () << __func__ << ":" << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_run_file (std::filesystem::path const& filename)
{
    try
    {
        std::ifstream fi (filename);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << filename << " for reading." << std::endl;
            return false;
        }

        std::vector<std::string> commands;

        if (filename.extension () == ".log")
        {
            for (std::string row; std::getline (fi, row); )
                if (auto mid = row.find (">"); mid != std::string::npos)
                {
                    row.erase (0, mid + 2);
                    commands.push_back (trim_both (row, ' '));
                }
        }
        else
        {
            for (std::string row; std::getline (fi, row); )
                commands.push_back (trim_both (row, ' '));
        }

        std::reverse (commands.begin (), commands.end ()); // jutsu, gonna pop vec's back
        console.commands.swap (commands);
    }
    catch (std::exception const& ex)
    {
        log () << __func__ << ":" << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
save_settings ()
{
    try
    {
        nlohmann::json json = {
            { "Log colors", {
                { "prompt", hex_string (console.prompt_color) },
                { "out", hex_string (console.out_color) },
                { "in", hex_string (console.in_color) },
            }},
            { "Help colors", {
                { "names", hex_string (console.help_names_color) },
                { "params", hex_string (console.help_params_color) },
                { "brief", hex_string (console.help_brief_color) },
                { "details", hex_string (console.help_details_color) },
            }},
            { "Execution delay", console.execution_delay }
        };

        save_font (json, console.gui_font);
        save_font (json, console.log_font);
        save_json (json, locations.settings);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_settings ()
{
    try
    {
        auto json = load_json (locations.settings);

        console.gui_font.name = "Default";
        console.gui_font.scale = 1.f;
        console.gui_font.size = 32.f;
        console.gui_font.color = IM_COL32_WHITE;
        console.gui_font.file = "";
        console.gui_font.default_data = font_inconsolata;
        load_font (json, console.gui_font);

        console.log_font.name = "Log";
        console.log_font.scale = 1.f;
        console.log_font.size = 32.f;
        console.log_font.color = IM_COL32_WHITE;
        console.log_font.file = "";
        console.log_font.default_data = font_inconsolata;
        load_font (json, console.log_font);

        console.prompt_color = IM_COL32 (0, 192, 0, 255);
        console.out_color = IM_COL32 (192, 192, 192, 255);
        console.in_color = IM_COL32 (192, 192, 192, 255);
        if (json.contains ("Log colors"))
        {
            auto const& j = json["Log colors"];
            console.prompt_color = std::stoul (
                    j.value ("prompt", hex_string (console.prompt_color)), nullptr, 0);
            console.out_color = std::stoul (
                    j.value ("out", hex_string (console.out_color)), nullptr, 0);
            console.in_color = std::stoul (
                    j.value ("in", hex_string (console.in_color)), nullptr, 0);
        }

        console.help_names_color = IM_COL32 (255, 255, 255, 255);
        console.help_params_color = IM_COL32 (128, 128, 128, 255);
        console.help_brief_color = IM_COL32 (192, 192, 192, 255);
        console.help_details_color = IM_COL32 (128, 128, 128, 255);
        if (json.contains ("Help colors"))
        {
            auto const& j = json["Help colors"];
            console.help_names_color = std::stoul (
                    j.value ("names", hex_string (console.help_names_color)), nullptr, 0);
            console.help_params_color = std::stoul (
                    j.value ("params", hex_string (console.help_params_color)), nullptr, 0);
            console.help_brief_color = std::stoul (
                    j.value ("brief", hex_string (console.help_brief_color)), nullptr, 0);
            console.help_details_color = std::stoul (
                    j.value ("details", hex_string (console.help_details_color)), nullptr, 0);
        }

        console.execution_delay = json.value ("Execution delay", 100);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
load_help_file (
        std::filesystem::path const& path,
        std::vector<std::string>& completers,
        std::vector<char> &data,
        std::vector<help_index>& indexes)
{
    try
    {

        auto append_to_help = [&data] (std::string& s, unsigned max_size)
        {
            if (s.size () > max_size)
            {
                log () << "Trimming down to " << max_size << " bytes: " << s << std::endl;
                s.resize (max_size);
            }
            data.insert (data.end (), s.cbegin (), s.cend ());
        };

        for (auto const& jcmd: load_json (path))
        {
            if (jcmd.contains ("version"))
                continue;

            help_index i;
            bool gotn = false;
            for (auto const& jn: jcmd["names"])
            {
                auto n = jn.get<std::string> ();
                if (trim_both (n, ' ').empty ())
                    continue;
                if (gotn)
                    n = " " + n; // To look better when displayed in the GUI
                else
                {
                    gotn = true;
                    i.begin = data.size ();
                }
                append_to_help (n, help_index::names_size);
                completers.emplace_back (std::move (n));
            }
            if (!gotn)
                throw std::runtime_error ("Missing valid 'names'.");

            i.params = data.size () - i.begin;
            if (auto s = jcmd.value ("params", std::string ()); trim_both (s, ' ').size ())
                append_to_help (s, help_index::params_size);

            i.brief = data.size () - (i.begin + i.params);
            if (auto s = jcmd.value ("brief", std::string ()); trim_both (s, ' ').size ())
                append_to_help (s, help_index::brief_size);

            i.details = data.size () - (i.begin + i.params + i.brief);
            if (auto s = jcmd.value ("details", std::string ()); trim_both (s, " \r\n").size ())
                append_to_help (s, help_index::details_size);

            i.end = data.size () - (i.begin + i.params + i.brief + i.details);
            indexes.push_back (i);
        }

        /// Likely SSO, so speed more or less is fine, nor are many completers expected
        std::sort (completers.begin (), completers.end ());
        completers.erase (std::unique (completers.begin (), completers.end ()), completers.end ());

        std::sort (indexes.begin (), indexes.end (), [&] (auto const& a, auto const& b) {
                return std::string_view (&data[a.begin], a.params)
                     < std::string_view (&data[b.begin], b.params);
        });
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load help file " << path << ": " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_help_files ()
{
    std::vector<std::string> completers;
    std::vector<char> data;
    std::vector<help_index> indexes;

    if (load_help_file (locations.help_sse, completers, data, indexes))
        console.sse_data.swap (data), console.sse_indexes.swap (indexes);
    else return false;

    data.clear (); indexes.clear ();

    if (load_help_file (locations.help_gui, completers, data, indexes))
        console.gui_data.swap (data), console.gui_indexes.swap (indexes);
    else return false;

    if (load_help_file (locations.help_alias, completers, data, indexes))
        console.alias_data.swap (data), console.alias_indexes.swap (indexes);

    console.completers.swap (completers);
    return true;
}

//--------------------------------------------------------------------------------------------------

