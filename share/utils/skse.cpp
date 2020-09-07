/**
 * @file skse.cpp
 * @brief Implements SKSE plugin for the current project.
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
 */

#include <sse-imgui/sse-imgui.h>
#include <sse-gui/sse-gui.h>
#include <sse-hooks/sse-hooks.h>
#include <utils/plugin.hpp>

#include <array>
#include <cstdint>
typedef std::uint32_t UInt32;
typedef std::uint64_t UInt64;
#include <skse/PluginAPI.h>

//--------------------------------------------------------------------------------------------------

/// Given by SKSE to uniquely identify this DLL
static PluginHandle plugin = 0;

/// To communicate with the other SKSE plugins.
static SKSEMessagingInterface* messages = nullptr;

/// [shared] Local initialization
sseimgui_api sseimgui = {};

/// [shared] Table with pointers
imgui_api imgui = {};

/// [shared] Local initialization
sseh_api sseh = {};

//--------------------------------------------------------------------------------------------------

static void
handle_sseimgui_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SSEIMGUI_API_VERSION)
    {
        log () << "Unsupported SSE-ImGui interface v" << m->type
               << " (it is not v" << SSEIMGUI_API_VERSION
                << "). Bailing out." << std::endl;
        return;
    }

    sseimgui = *reinterpret_cast<sseimgui_api*> (m->data);

    int maj;
    sseimgui.version (nullptr, &maj, nullptr, nullptr);
    if (maj < 1)
    {
        log () << "SSE-MapTrack needs SSE-ImGui 1.1 or later." << std::endl;
        return;
    }

    imgui = sseimgui.make_imgui_api ();
    log () << "Accepted SSE-ImGui interface v" << SSEIMGUI_API_VERSION << std::endl;

    extern bool setup ();
    if (!setup ())
    {
        log () << "Unable to initialize " << plugin_name () << std::endl;
        return;
    }

    extern void render (int);
    sseimgui.render_listener (&render, 0);
    log () << "All done." << std::endl;
}

//--------------------------------------------------------------------------------------------------

static void
handle_sseh_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SSEH_API_VERSION)
    {
        log () << "Unsupported SSEH interface v" << m->type
               << " (it is not v" << SSEH_API_VERSION
                << "). Bailing out." << std::endl;
        return;
    }
    sseh = *reinterpret_cast<sseh_api*> (m->data);
    log () << "Accepted SSEH interface v" << SSEH_API_VERSION << std::endl;
}

//--------------------------------------------------------------------------------------------------

/// Post Load ensure SSE-ImGui is loaded and can accept listeners

static void
handle_skse_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SKSEMessagingInterface::kMessage_PostLoad)
        return;
    log () << "SKSE Post Load." << std::endl;
    messages->RegisterListener (plugin, "SSEH", handle_sseh_message);
    messages->RegisterListener (plugin, "SSEIMGUI", handle_sseimgui_message);
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Query (SKSEInterface const* skse, PluginInfo* info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = plugin_name ().c_str ();
    plugin_version ((int*) &info->version, nullptr, nullptr, nullptr);

    plugin = skse->GetPluginHandle ();

    if (skse->isEditor)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Load (SKSEInterface const* skse)
{
    open_log (plugin_name ());

    messages = (SKSEMessagingInterface*) skse->QueryInterface (kInterface_Messaging);
    messages->RegisterListener (plugin, "SKSE", handle_skse_message);

    int a, m, p;
    const char* b;
    plugin_version (&a, &m, &p, &b);
    log () << plugin_name () <<' '<< a <<'.'<< m <<'.'<< p <<" ("<< b <<')' << std::endl;

    return true;
}

//--------------------------------------------------------------------------------------------------


