/**
 * @file plugin.cpp
 * @internal
 *
 * This file is part of General Utilities project (aka Utils).
 *
 *   Utils is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Utils is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Utils If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Utilities
 */

#include <utils/plugin.hpp>
#include <utils/winutils.hpp>

#include <iomanip>
#include <chrono>

/// Log file in pre-defined location
static std::ofstream logfile;

//--------------------------------------------------------------------------------------------------

void
plugin_version (int* maj, int* min, int* patch, const char** timestamp)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (maj) *maj = ver[0];
    if (min) *min = ver[1];
    if (patch) *patch = ver[2];
    if (timestamp) *timestamp = PLUGIN_TIMESTAMP; //"2019-04-15T08:37:11.419416+00:00"
}

//--------------------------------------------------------------------------------------------------

std::string const& plugin_directory ()
{
    static std::string v = "data\\skse\\plugins\\" + plugin_name () + "\\";
    return v;
}

//--------------------------------------------------------------------------------------------------

void
open_log (std::string const& basename)
{
    std::string destination = "";
    if (known_folder_path (FOLDERID_Documents, destination))
    {
        // Before plugins are loaded, SKSE takes care to create the directiories
        destination += "\\My Games\\Skyrim Special Edition\\SKSE\\";
    }
    destination += basename + ".log";
    logfile.open (destination);
}

//--------------------------------------------------------------------------------------------------

std::ofstream&
log ()
{
    // MinGW 4.9.1 have no std::put_time()
    // TODO: Already using 9+ so there might be one
    using std::chrono::system_clock;
    auto now_c = system_clock::to_time_t (system_clock::now ());
    auto loc_c = std::localtime (&now_c);
    logfile << '['
            << 1900 + loc_c->tm_year
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mon
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mday
            << ' ' << std::setw (2) << std::setfill ('0') << loc_c->tm_hour
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_min
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_sec
        << "] ";
    return logfile;
}

//--------------------------------------------------------------------------------------------------

std::uintptr_t
skyrim_base ()
{
    return reinterpret_cast<std::uintptr_t> (::GetModuleHandle (nullptr));
}

//--------------------------------------------------------------------------------------------------

