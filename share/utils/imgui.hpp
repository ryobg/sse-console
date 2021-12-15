/**
 * @file imgui.hpp
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

#ifndef UTILS_IMGUI_HPP
#define UTILS_IMGUI_HPP

#include <utils/plugin.hpp>
#include <sse-imgui/sse-imgui.h>
#include <sse-imgui/imgui_wrapped.h>
#include <filesystem>

//--------------------------------------------------------------------------------------------------

/// Define and setup in the utils/skse.cpp
extern imgui_api imgui;
extern sseimgui_api sseimgui;

/// Defined in inconsolata.cpp
extern const char* font_inconsolata;

//--------------------------------------------------------------------------------------------------

struct font_t
{
    std::string name;
    float scale;
    float size;
    std::uint32_t color;
    std::string file;
    const char* default_data;
    ImFont* imfont;             ///< Actual font with its settings (apart from #color)
};

//--------------------------------------------------------------------------------------------------

void load_font (nlohmann::json const& json, font_t& font);
void save_font (nlohmann::json& json, font_t const& font);

//--------------------------------------------------------------------------------------------------

void render_font_settings (font_t& font, bool render_color = true);
void render_color_setting (const char* name, std::uint32_t& color);

//--------------------------------------------------------------------------------------------------

class render_load_files
{
    bool show;
    bool open;
    std::string title;
    std::vector<std::string> extensions;
    std::vector<std::string> names;
    int selection;
    float height_hint;

public:
    void init (std::string_view title, std::initializer_list<const char*> extensions);
    ImVec2 button_size; ///< Damn important
    void queue_render () {
        show = true;
        open = true;
    }
    std::filesystem::path update (std::filesystem::path const& folder = plugin_directory ());
};

//--------------------------------------------------------------------------------------------------

/// Turns out there is nice black & white, high contrast theme used across the mods (no fonts)

class default_theme
{
public:
    default_theme ();
    ~ default_theme ();
    default_theme (default_theme const&) = delete;
    default_theme (default_theme&&) = delete;
    default_theme& operator = (default_theme const&) = delete;
    default_theme& operator = (default_theme&&) = delete;
};

//--------------------------------------------------------------------------------------------------

#endif

