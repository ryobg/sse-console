/**
 * @file imgui.cpp
 * @brief Common across plugins ImGui tools
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

#include <utils/imgui.hpp>
#include <utils/winutils.hpp>

constexpr int color_widget_flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayHSV
    | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar
    | ImGuiColorEditFlags_AlphaBar;

//--------------------------------------------------------------------------------------------------

default_theme::default_theme ()
{
    imgui.igPushStyleColor_Vec4 (ImGuiCol_FrameBg,        ImVec4 {0, 0, 0, 0});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_Button,         ImVec4 {0, 0, 0, 0});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_TitleBgActive,  ImVec4 {0, 0, 0, 1.f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_CheckMark,      ImVec4 {.61f, .61f, .61f, 1.f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_SliderGrab,     ImVec4 {.61f, .61f, .61f, 1.f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_ResizeGrip,     ImVec4 {.61f, .61f, .61f, 1.f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_TextSelectedBg, ImVec4 {.61f, .61f, .61f, 1.f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_ButtonHovered , ImVec4 {0.26f, 0.59f, 0.98f, 0.4f});
    imgui.igPushStyleColor_Vec4 (ImGuiCol_FrameBgHovered, ImVec4 {0.26f, 0.59f, 0.98f, 0.4f});

    imgui.igPushStyleVar_Vec2 (ImGuiStyleVar_ItemSpacing, ImVec2 {5, 10});
    imgui.igPushStyleVar_Vec2 (ImGuiStyleVar_FramePadding, ImVec2 {5, 5});
    imgui.igPushStyleVar_Float (ImGuiStyleVar_FrameBorderSize, 1.f);
    imgui.igPushStyleVar_Float (ImGuiStyleVar_WindowBorderSize, 0.f);
}

default_theme::~ default_theme ()
{
    imgui.igPopStyleVar (4);
    imgui.igPopStyleColor (9);
}

//--------------------------------------------------------------------------------------------------

void
render_font_settings (font_t& font, bool render_color)
{

    // Likely SSOs
    auto name = font.name + " font:";
    imgui.igText (name.c_str ());

    if (render_color)
    {
        auto color = "Color##" + font.name;
        ImVec4 col;
        imgui.igColorConvertU32ToFloat4 (&col, font.color);
        if (imgui.igColorEdit4 (color.c_str (), (float*) &col, color_widget_flags))
            font.color = imgui.igGetColorU32_Vec4 (col);
    }

    auto scale = "Scale##" + font.name;
    imgui.igSliderFloat (scale.c_str (), &font.imfont->Scale, .5f, 2.f, "%.2f", 1);
}

//--------------------------------------------------------------------------------------------------

void
render_color_setting (const char* name, std::uint32_t& color)
{
    ImVec4 c;
    imgui.igColorConvertU32ToFloat4 (&c, color);
    if (imgui.igColorEdit4 (name, (float*) &c, color_widget_flags))
        color = imgui.igGetColorU32_Vec4 (c);
}

//--------------------------------------------------------------------------------------------------

void render_load_files::init (std::string_view t, std::initializer_list<const char*> e)
{
    title = t;
    extensions.assign (e.begin (), e.end ());
    names.clear ();
    show = false;
    open = false;
    selection = -1;
    height_hint = -1;
    button_size = ImVec2 {0, 0};
}

//--------------------------------------------------------------------------------------------------

static bool
extract_vector_string (void* data, int idx, const char** out_text)
{
    auto vars = reinterpret_cast<std::vector<std::string>*> (data);
    *out_text = vars->at (idx).c_str ();
    return true;
}

std::filesystem::path render_load_files::update (std::filesystem::path const& folder)
{
    if (!show)
        return "";

    if (open)
    {
        open = false;
        names.clear ();
        for (auto const& x: extensions)
        {
            auto b = names.size ();
            enumerate_files (folder.string () + "*" + x, names);
            auto e = names.size ();
            std::sort (names.begin () + b, names.begin () + e);
        }
        if (extensions.size () == 1)
            for (auto& n: names) n = n.substr (0, n.size () - extensions[0].size ());
    }

    std::filesystem::path target;
    if (imgui.igBegin (title.c_str (), &show, 0))
    {
        imgui.igText (folder.string ().c_str ());
        ImVec2 namessz;
        imgui.igGetContentRegionAvail (&namessz);
        namessz.x -= button_size.x + imgui.igGetStyle ()->ItemSpacing.x;
        imgui.igSetNextItemWidth (namessz.x);
        imgui.igBeginGroup ();
        imgui.igListBox_FnBoolPtr ("##Names",
                &selection, extract_vector_string, &names, int (names.size ()), height_hint);
        imgui.igEndGroup ();
        imgui.igSameLine (0, -1);
        imgui.igBeginGroup ();
        if (imgui.igButton ("Load", button_size) && unsigned (selection) < names.size ())
        {
            show = false;
            target = names[selection] + (extensions.size () == 1 ? extensions[0] : std::string ());
        }
        if (imgui.igButton ("Cancel", button_size))
            show = false;
        imgui.igEndGroup ();
        height_hint = (imgui.igGetWindowHeight () / imgui.igGetTextLineHeightWithSpacing ()) - 3;
    }
    imgui.igEnd ();
    return target;
}

//--------------------------------------------------------------------------------------------------

