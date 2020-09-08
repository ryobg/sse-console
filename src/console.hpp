/**
 * @file console.hpp
 * @brief Shared interface between files in SSE-Console
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

#ifndef SSE_CONSOLE_HPP
#define SSE_CONSOLE_HPP

#include <utils/imgui.hpp>
#include <utils/misc.hpp>
#include <vector>
#include <filesystem>

//--------------------------------------------------------------------------------------------------

struct log_index
{
    std::uint32_t begin;    ///< Offset within console_t#log_data
    std::uint32_t out : 1;  ///< True if outgoing, otherwise incoming log message
    std::uint32_t mid : 7;  ///< Relative position of the actual message i.e. after the prompt
    std::uint32_t end : 24; ///< Relative one-past-the end. More size, but easier to work with
};
static_assert (sizeof (log_index) == 8);

/// The opposite of what #record_log_message() does wrt to encoding the #log_index
static inline auto
extract_message (std::vector<char> const& source, log_index i)
{
    return std::make_tuple (&source[i.begin], &source[i.begin + i.mid], &source[i.begin + i.end]);
}

/// Adds a prompt and puts into console#log_data and console#log_indexes
void record_log_message (bool outgoing, std::string const& msg);

//--------------------------------------------------------------------------------------------------

/// Compressed start of record, holding relative to each other offsets
struct help_index
{
    enum {
        names_bits   = 6,  names_size   = 1 << names_bits,
        params_bits  = 6,  params_size  = 1 << params_bits,
        brief_bits   = 7,  brief_size   = 1 << brief_bits,
        details_bits = 11, details_size = 1 << details_bits,
        waste_bits   = 2,  waste_size   = 1 << waste_bits
    };
    std::uint32_t begin  ;     ///< Offset within console_t#help_dat and start of names
    std::uint32_t params : names_bits;
    std::uint32_t brief  : params_bits;
    std::uint32_t details: brief_bits;
    std::uint32_t end    : details_bits;
    std::uint32_t waste  : waste_bits;
};
static_assert (sizeof (help_index) == 8);

static inline auto
extract_message (std::vector<char> const& source, help_index i)
{
    return std::make_tuple (
            &source[i.begin],
            &source[i.begin + i.params],
            &source[i.begin + i.params + i.brief],
            &source[i.begin + i.params + i.brief + i.details],
            &source[i.begin + i.params + i.brief + i.details + i.end]
    );
}

//--------------------------------------------------------------------------------------------------

struct console_t
{
    font_t gui_font, log_font;
    std::uint32_t prompt_color, out_color, in_color;

    std::vector<char> log_data;         ///< Whole, unfiltered buffer, full of terminated records
    std::vector<log_index> log_indexes; ///< Compressed index for access to #log_data
    int counter_in, counter_out;

    std::vector<std::string> completers; ///< Used in auto-completion

    std::uint32_t help_names_color, help_params_color, help_brief_color, help_details_color;

    std::vector<char> sse_data, gui_data, alias_data;
    std::vector<help_index> sse_indexes, gui_indexes, alias_indexes;

    std::vector<std::string> commands;  ///< Queue of commands currently running
    int execution_delay;                ///< In milliseconds, wrt to #commands
};

extern console_t console;

bool save_log_file (std::filesystem::path const& filename);
bool load_log_file (std::filesystem::path const& filename);
bool load_run_file (std::filesystem::path const& filename);
bool load_settings ();
bool save_settings ();
bool load_help_files ();

bool setup ();
bool setup_render ();

//--------------------------------------------------------------------------------------------------

class skyrim_log {
public:
    static void print (const char* format, ...);
    static std::string last_message ();
    static void last_message (std::string const&);
};

class skyrim_console {
public:
    static void execute (std::string const& message);
    static std::uint32_t selected_form ();
};

void setup_hooks ();

//--------------------------------------------------------------------------------------------------

static inline std::string
uppercase_string (std::string s)
{
    for (char& c: s) c = (char) std::toupper (c & 0x7f);
    return s;
}

//--------------------------------------------------------------------------------------------------

/// Cascaded filtering of indexes based on #log_index.

template<class IndexT>
class records_filter
{
public:

    void init (
            std::vector<char> const* text,
            std::vector<IndexT> const* indexes,
            std::initializer_list<int> segments)
    {
        chars.clear ();
        chars.resize (segments.size ());
        filters.clear ();
        filters.resize (segments.size ());
        splits.clear ();
        splits.insert (splits.end (), segments.begin (), segments.end ());
        source_filter = indexes;
        source_text = text;
        current_filter = source_filter;
        buffer.clear ();
        buffer.resize (256, '\0');
    }

    /// When the indexes/text are reset outside
    void clear ()
    {
        current_filter = source_filter;
        std::fill (chars.begin (), chars.end (), "");
        std::fill (filters.begin (), filters.end (), std::vector<IndexT> ());
    }

    void update (const char* filter_text, bool force_update = false)
    {
        auto text = uppercase_string (trimmed_both (filter_text, ' '));

        current_filter = source_filter;
        if (text.size () < splits[0] || source_filter->size () < 2)
            return;

        for (std::size_t i = 0, n = filters.size (); i < n; ++i)
        {
            if (force_update || text.size () >= splits[i])
            {
                auto text_end = i+1 == n ? text.size () : splits[i];

                if (text.compare (0, text_end, chars[i]))
                {
                    chars[i] = text.substr (0, text_end);
                    filter (filters[i], chars[i].data ());
                }
                current_filter = &filters[i];
            }
        }
    }

    std::vector<char> buffer;   ///< Moved in here the GUI input text field storage

    std::vector<IndexT> const* current_indexes () const {
        return current_filter;
    }

    std::vector<char> const* source_data () const {
        return source_text;
    }

private:

    std::vector<IndexT> const* current_filter;
    std::vector<char> const* source_text;
    std::vector<IndexT> const* source_filter;
    std::vector<std::string> chars;
    std::vector<std::vector<IndexT>> filters;
    std::vector<std::size_t> splits;

    // Go through the real source of text and find matches
    void filter (std::vector<IndexT>& dst, const char* txt)
    {
        dst.clear ();
        auto s = uppercase_string (txt);
        std::copy_if (current_filter->cbegin (), current_filter->cend (), std::back_inserter (dst),
                [&s, this] (IndexT const& n)
        {
            auto t = extract_message (*source_text, n);
            auto b = std::get<0> (t);
            auto e = std::get<std::tuple_size<decltype(t)>::value - 1> (t);
            auto u = uppercase_string (std::string (b, e));
            return u.find (s) != std::string::npos;
        });
    };
};

//--------------------------------------------------------------------------------------------------

#endif //SSE_CONSOLE_HPP

