/**
 * @file misc.hpp
 * @brief A mix of tools which does not fit any other particular category
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
 */

#ifndef UTILS_MISC_HPP
#define UTILS_MISC_HPP

#include <string>

//--------------------------------------------------------------------------------------------------

// trim from end of string (right)
template<class CharT, class T>
constexpr std::basic_string<CharT>&
trim_end (std::basic_string<CharT>& s, T const& t)
{
    s.erase (s.find_last_not_of (t) + 1);
    return s;
}

// trim from beginning of string (left)
template<class CharT, class T>
constexpr std::basic_string<CharT>&
trim_begin (std::basic_string<CharT>& s, T const& t)
{
    s.erase (0, s.find_first_not_of (t));
    return s;
}

// trim from both ends of string (right then left)
template<class CharT, class T>
constexpr std::basic_string<CharT>&
trim_both (std::basic_string<CharT>& s, T const& t)
{
    return trim_begin (trim_end (s, t), t);
}

template<class S, class T>
constexpr auto
trimmed_both (S const& s, T const& t)
{
    std::basic_string b (s);
    return trim_begin (trim_end (b, t), t);
}

//--------------------------------------------------------------------------------------------------

#endif

