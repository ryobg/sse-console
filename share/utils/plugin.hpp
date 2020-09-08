/**
 * @file plugin.hpp
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
 *
 * @details
 * Common functions which seems to be used across all the plugin projects currently based on Skyrim
 * SE, SKSE and especially used in SSE-Hooks/GUI/ImGui/MapTrack/Journal/Console.
 *
 * Some of the functions use static storage.
 */

#ifndef UTILS_PLUGIN_HPP
#define UTILS_PLUGIN_HPP

// Warning come in a BSON parser, which is not used, and probably shouldn't be
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat="
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  include <nlohmann/json.hpp>
#  pragma GCC diagnostic pop
#endif

#include <string>
#include <fstream>
#include <array>
#include <filesystem>

//--------------------------------------------------------------------------------------------------

/// Requires X.Y.Z ../VERSION file and PLUGIN_TIMESTAMP as ISO datetime string

void plugin_version (int* maj, int* min, int* patch, const char** timestamp);

/// Where is the personal storage for this plugin (e.g. relative data/skse/plugins/sse-maptrack/)

std::string const& plugin_directory ();

/// Defined in the plugin itself, returns the name (e.g. sse-console, sse-maptrack, etc.)

std::string const& plugin_name ();

//--------------------------------------------------------------------------------------------------

/**
 * Opens a simple text file in default for SKSE plugins location.
 *
 * @param basename of the file
 */

void open_log (std::string const& basename);

/**
 * Prepends a header and returns an object for further record appending of one record.
 *
 * @returns the output log stream, finalize that one record with std::endl
 */

std::ofstream& log ();

//--------------------------------------------------------------------------------------------------

/// Turn relative addresses into absolute so that the Skyrim watch points can be set.

std::uintptr_t skyrim_base ();

/// Obtains an address to a relative object, to a relative object, to a relative object, to a...

template<class T, int N>
struct relocation
{
    std::array<std::uintptr_t, N> offsets;
    T obtain () const
    {
        std::uintptr_t that = skyrim_base ();
        for (int i = 0; i < N-1; ++i)
        {
            that = *reinterpret_cast<std::uintptr_t*> (that + offsets[i]);
            if (!that) return nullptr;
        }
        return reinterpret_cast<T> (that + offsets[N-1]);
    }
};

//--------------------------------------------------------------------------------------------------

/// Adds also the #plugin_version() info
void save_json (nlohmann::json& json, std::filesystem::path const& file);

/// Empty if no file, throws on bad json though.
nlohmann::json load_json (std::filesystem::path const& file);

//--------------------------------------------------------------------------------------------------

#endif

