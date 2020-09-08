/**
 * @file file.cpp
 * @brief Common across plugins JSON and ImGui file ops.
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

#include <fstream>
#include <gsl/gsl_util>
#include <utils/imgui.hpp>
#include <utils/winutils.hpp>

//--------------------------------------------------------------------------------------------------

void
save_json (nlohmann::json& json, std::filesystem::path const& file)
{
    int maj, min, patch;
    const char* timestamp;
    plugin_version (&maj, &min, &patch, &timestamp);

    try
    {
        auto& j = json["version"];
        j["major"] = maj;
        j["minor"] = min;
        j["patch"] = patch;
        j["timestamp"] = timestamp;

        std::ofstream of (file);
        if (!of.is_open ())
            log () << "Unable to open " << file << " for writting." << std::endl;
        else
            of << json.dump (4);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save " << file << " as JSON: " << ex.what () << std::endl;
        throw ex;
    }
}

//--------------------------------------------------------------------------------------------------

nlohmann::json
load_json (std::filesystem::path const& file)
{
    try
    {
        nlohmann::json json = nlohmann::json::object ();
        std::ifstream fi (file);
        if (!fi.is_open ())
            log () << "Unable to open " << file << " for reading." << std::endl;
        else
            fi >> json;
        return json;
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to parse " << file << " as JSON: " << ex.what () << std::endl;
        throw ex;
    }
}

//--------------------------------------------------------------------------------------------------

void
load_font (nlohmann::json const& json, font_t& font)
{
    std::string section = font.name + " font";
    nlohmann::json jf;
    if (json.contains (section))
        jf = json[section];
    else jf = nlohmann::json::object ();

    font.color = std::stoull (jf.value ("color", hex_string (font.color)), nullptr, 0);
    font.scale = jf.value ("scale", font.scale);
    auto set_scale = gsl::finally ([&font] { font.imfont->Scale = font.scale; });

    // The UI load button, otherwise we have to recreate all fonts outside the rendering loop
    // and thats too much of hassle if not tuning all other params through the UI too.
    if (font.imfont)
        return;

    font.size = jf.value ("size", font.size);
    font.file = jf.value ("file", plugin_directory () + font.name + ".ttf");
    if (font.file.empty ())
        font.file = plugin_directory ()+ font.name + ".ttf";

    auto font_atlas = imgui.igGetIO ()->Fonts;

    if (file_exists (font.file))
        font.imfont = imgui.ImFontAtlas_AddFontFromFileTTF (
                font_atlas, font.file.c_str (), font.size, nullptr, nullptr);
    if (!font.imfont)
    {
        font.imfont = imgui.ImFontAtlas_AddFontFromMemoryCompressedBase85TTF (
            font_atlas, font.default_data, font.size, nullptr, nullptr);
        font.file.clear ();
    }
}

//--------------------------------------------------------------------------------------------------

void
save_font (nlohmann::json& json, font_t const& font)
{
    auto& jf = json[font.name + " font"];
    jf["scale"] = font.imfont->Scale;
    jf["color"] = hex_string (font.color);
    jf["size"] = font.imfont->FontSize;
    jf["file"] = font.file;
}

//--------------------------------------------------------------------------------------------------

