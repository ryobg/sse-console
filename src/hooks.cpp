/**
 * @file hooks.cpp
 * @brief Sniff the Skyrim for Console support
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

#include <cstdarg>
#include <utils/winutils.hpp>
#include <utils/plugin.hpp>
#include <sse-hooks/sse-hooks.h>
#include "console.hpp"

//--------------------------------------------------------------------------------------------------

struct skyrim_log_rels
{
    /// 515064 0x2f000f0
    relocation<void*, 2> owner { 0x2f000f0, 0 };
    relocation<char*, 2> last { 0x2f000f0, 1 };
    /// 50180 0x85c2c0
    relocation<void*, 1> vprint { 0x85c2c0 };
};

static skyrim_log_rels logrels;

//--------------------------------------------------------------------------------------------------

void skyrim_log::print (const char* format, ...)
{
    void* p = logrels.owner.obtain ();
    void* f = logrels.vprint.obtain ();
    if (!p || !f)
    {
        log () << "Unable to obtain console log: owner|print." << std::endl;
        return;
    }

    std::va_list args;
    va_start (args, format);
    ((void(*)(void*, const char*, std::va_list)) f) (p, format, args);
    va_end (args);
};

//--------------------------------------------------------------------------------------------------

std::string skyrim_log::last_message ()
{
    char const* p = logrels.last.obtain ();
    if (!p)
    {
        log () << "Unable to obtain console interface: last message." << std::endl;
        return "";
    }
    return p; //max 0x400?
}

//--------------------------------------------------------------------------------------------------

void skyrim_log::last_message (std::string const& msg)
{
    char* p = logrels.last.obtain ();
    if (!p)
    {
        log () << "Unable to obtain console interface: last message." << std::endl;
        return;
    }
    *std::copy_n (msg.cbegin (), std::min (0x399, int (msg.size ())), p) = '\0';
}

//--------------------------------------------------------------------------------------------------

struct skyrim_console_rels
{
    // 514349 0x1ec3cb3
    relocation<bool*, 1> factories_enabled { 0x1ec3cb3 };
    // 514355 0x1ec3ce0
    relocation<void*, 2> script_factory { 0x1ec3ce0 + 19 * sizeof (void*), 0 };
    //519394 2f4c31c
    relocation<void*, 2> selected_ref { 0x2f4c31c, 0 };
    //12204 1329d0
    relocation<bool(*)(void**,void**), 1> make_smart_pointer { 0x1329d0 };
    // 21416 2e75f0
    relocation<void(*)(void*,char*,int,void*), 1> compile_and_run { 0x2e75f0 };
};

static skyrim_console_rels conrels;

//--------------------------------------------------------------------------------------------------

static inline void*
create_script ()
{
    if (bool* e = conrels.factories_enabled.obtain (); !e || !*e)
        return nullptr;
    void* f = conrels.script_factory.obtain ();
    if (!f)
        return nullptr;
    auto vtbl = *((std::uintptr_t**) f);
    auto create = (void*(*)(void*)) vtbl[1];
    return create (f);
}

//--------------------------------------------------------------------------------------------------

static inline void
destroy_script (void* s)
{
    auto vtbl = *((std::uintptr_t**) s);
    auto dtor = (void(*)(void*, std::size_t)) vtbl[0];
    dtor (s, 1);
}

//--------------------------------------------------------------------------------------------------

static inline void*
create_smart (void* ref)
{
    void* p = nullptr;
    if (auto f = conrels.make_smart_pointer.obtain (); f)
        f (&ref, &p);
    return p;
}

//--------------------------------------------------------------------------------------------------

static inline void
destroy_smart (void* p)
{
    if (p)
    {
        auto pref = std::uintptr_t (p) + 32;        // 4th parent being a reference counter
        auto rcnt = (volatile long*) (pref + 8);    // his 2nd member being the counter itself
        if ((_InterlockedDecrement (rcnt) & 0x3ff) == 0)
        {
            auto vtbl = std::uintptr_t (*(void**) pref); // follow up to that parent
            auto dthi = (void(*)(void*)) (vtbl + 8);     // take his 2nd function, a custom delete
            dthi ((void*) vtbl);                         // call it with its 'this' pointer
        }
    }
}

//--------------------------------------------------------------------------------------------------

static void
assign_buffer (void* script, std::string const& msg)
{
    char** txt = (char**) &(((char*) script)[0x38]);

    if (msg.empty ())
    {
        *txt = nullptr;
        return;
    }

    static std::vector<char> buff;
    buff.resize (msg.size () + 1);
    *std::copy (msg.begin (), msg.end (), buff.begin ()) = '\0';
    *txt = buff.data ();
}

//--------------------------------------------------------------------------------------------------

static inline void
run (void* script, void* ref)
{
    char c;
    if (auto f = conrels.compile_and_run.obtain (); f)
        f (script, &c, 1, ref);
}

//--------------------------------------------------------------------------------------------------

void skyrim_console::execute (std::string const& message)
{
    void* script = create_script ();
    if (!script)
        return;
    void* selref = create_smart (conrels.selected_ref.obtain ());
    assign_buffer (script, message);
    run (script, selref);
    assign_buffer (script, "");
    destroy_script (script);
    destroy_smart (selref);
}

//--------------------------------------------------------------------------------------------------

std::uint32_t skyrim_console::selected_form ()
{
    if (void* selref = create_smart (conrels.selected_ref.obtain ()); selref)
    {
        auto a = std::uintptr_t (selref) + 0x14; // form id address within the object
        auto r = *(std::uint32_t*) a;
        destroy_smart (selref);
        return r;
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------

void setup_hooks ()
{
    extern sseh_api sseh; //skse.cpp

    sseh.find_target ("ConsoleLog", &logrels.owner.offsets[0]);
    sseh.find_target ("ConsoleLog.VPrint", &logrels.vprint.offsets[0]);
    logrels.last.offsets[0] = logrels.owner.offsets[0];

    if (sseh.find_target ("FormFactories", &conrels.script_factory.offsets[0]))
        conrels.script_factory.offsets[0] += 19 * sizeof (void*);
    sseh.find_target ("FormFactories.Enabled", &conrels.factories_enabled.offsets[0]);
    sseh.find_target ("Console.SelectedReference", &conrels.selected_ref.offsets[0]);
    sseh.find_target ("HandleManager.SmartPointer", &conrels.make_smart_pointer.offsets[0]);
    sseh.find_target ("Script.CompileRun", &conrels.compile_and_run.offsets[0]);
}

//--------------------------------------------------------------------------------------------------

