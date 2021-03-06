#pragma once

// Distributed under the 3-Clause BSD License.  See accompanying
// file LICENSE or https://github.com/CLIUtils/CLI11 for details.

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace CLI {

/// Include the items in this namespace to get free conversion of enums to/from streams.
/// (This is available inside CLI as well, so CLI11 will use this without a using statement).
namespace enums {

/// output streaming for enumerations
template <typename T, typename = typename std::enable_if<std::is_enum<T>::value>::type>
std::ostream &operator<<(std::ostream &in, const T &item) {
    // make sure this is out of the detail namespace otherwise it won't be found when needed
    return in << static_cast<typename std::underlying_type<T>::type>(item);
}

/// input streaming for enumerations
template <typename T, typename = typename std::enable_if<std::is_enum<T>::value>::type>
std::istream &operator>>(std::istream &in, T &item) {
    typename std::underlying_type<T>::type i;
    in >> i;
    item = static_cast<T>(i);
    return in;
}
} // namespace enums

/// Export to CLI namespace
using namespace enums;

namespace detail {

// Based on http://stackoverflow.com/questions/236129/split-a-string-in-c
/// Split a string by a delim
inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    // Check to see if empty string, give consistent result
    if(s.empty())
        elems.emplace_back();
    else {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while(std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
    }
    return elems;
}
/// simple utility to convert various types to a string
template <typename T> inline std::string as_string(const T &v) {
    std::ostringstream s;
    s << v;
    return s.str();
}
// if the data type is already a string just forward it
template <typename T, typename = typename std::enable_if<std::is_constructible<std::string, T>::value>::type>
inline auto as_string(T &&v) -> decltype(std::forward<T>(v)) {
    return std::forward<T>(v);
}

/// Simple function to join a string
template <typename T> std::string join(const T &v, std::string delim = ",") {
    std::ostringstream s;
    auto beg = std::begin(v);
    auto end = std::end(v);
    if(beg != end)
        s << *beg++;
    while(beg != end) {
        s << delim << *beg++;
    }
    return s.str();
}

/// Simple function to join a string from processed elements
template <typename T,
          typename Callable,
          typename = typename std::enable_if<!std::is_constructible<std::string, Callable>::value>::type>
std::string join(const T &v, Callable func, std::string delim = ",") {
    std::ostringstream s;
    auto beg = std::begin(v);
    auto end = std::end(v);
    if(beg != end)
        s << func(*beg++);
    while(beg != end) {
        s << delim << func(*beg++);
    }
    return s.str();
}

/// Join a string in reverse order
template <typename T> std::string rjoin(const T &v, std::string delim = ",") {
    std::ostringstream s;
    for(size_t start = 0; start < v.size(); start++) {
        if(start > 0)
            s << delim;
        s << v[v.size() - start - 1];
    }
    return s.str();
}

// Based roughly on http://stackoverflow.com/questions/25829143/c-trim-whitespace-from-a-string

/// Trim whitespace from left of string
inline std::string &ltrim(std::string &str) {
    auto it = std::find_if(str.begin(), str.end(), [](char ch) { return !std::isspace<char>(ch, std::locale()); });
    str.erase(str.begin(), it);
    return str;
}

/// Trim anything from left of string
inline std::string &ltrim(std::string &str, const std::string &filter) {
    auto it = std::find_if(str.begin(), str.end(), [&filter](char ch) { return filter.find(ch) == std::string::npos; });
    str.erase(str.begin(), it);
    return str;
}

/// Trim whitespace from right of string
inline std::string &rtrim(std::string &str) {
    auto it = std::find_if(str.rbegin(), str.rend(), [](char ch) { return !std::isspace<char>(ch, std::locale()); });
    str.erase(it.base(), str.end());
    return str;
}

/// Trim anything from right of string
inline std::string &rtrim(std::string &str, const std::string &filter) {
    auto it =
        std::find_if(str.rbegin(), str.rend(), [&filter](char ch) { return filter.find(ch) == std::string::npos; });
    str.erase(it.base(), str.end());
    return str;
}

/// Trim whitespace from string
inline std::string &trim(std::string &str) { return ltrim(rtrim(str)); }

/// Trim anything from string
inline std::string &trim(std::string &str, const std::string filter) { return ltrim(rtrim(str, filter), filter); }

/// Make a copy of the string and then trim it
inline std::string trim_copy(const std::string &str) {
    std::string s = str;
    return trim(s);
}

/// Make a copy of the string and then trim it, any filter string can be used (any char in string is filtered)
inline std::string trim_copy(const std::string &str, const std::string &filter) {
    std::string s = str;
    return trim(s, filter);
}
/// Print a two part "help" string
inline std::ostream &format_help(std::ostream &out, std::string name, std::string description, size_t wid) {
    name = "  " + name;
    out << std::setw(static_cast<int>(wid)) << std::left << name;
    if(!description.empty()) {
        if(name.length() >= wid)
            out << "\n" << std::setw(static_cast<int>(wid)) << "";
        out << description;
    }
    out << "\n";
    return out;
}

/// Verify the first character of an option
template <typename T> bool valid_first_char(T c) { return std::isalnum(c, std::locale()) || c == '_'; }

/// Verify following characters of an option
template <typename T> bool valid_later_char(T c) {
    return std::isalnum(c, std::locale()) || c == '_' || c == '.' || c == '-';
}

/// Verify an option name
inline bool valid_name_string(const std::string &str) {
    if(str.empty() || !valid_first_char(str[0]))
        return false;
    for(auto c : str.substr(1))
        if(!valid_later_char(c))
            return false;
    return true;
}

/// Return a lower case version of a string
inline std::string to_lower(std::string str) {
    std::transform(std::begin(str), std::end(str), std::begin(str), [](const std::string::value_type &x) {
        return std::tolower(x, std::locale());
    });
    return str;
}

/// remove underscores from a string
inline std::string remove_underscore(std::string str) {
    str.erase(std::remove(std::begin(str), std::end(str), '_'), std::end(str));
    return str;
}

/// Find and replace a substring with another substring
inline std::string find_and_replace(std::string str, std::string from, std::string to) {

    size_t start_pos = 0;

    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    return str;
}

/// check if the flag definitions has possible false flags
inline bool has_default_flag_values(const std::string &flags) {
    return (flags.find_first_of("{!") != std::string::npos);
}

inline void remove_default_flag_values(std::string &flags) {
    size_t loc = flags.find_first_of('{');
    while(loc != std::string::npos) {
        auto finish = flags.find_first_of("},", loc + 1);
        if((finish != std::string::npos) && (flags[finish] == '}')) {
            flags.erase(flags.begin() + loc, flags.begin() + finish + 1);
        }
        loc = flags.find_first_of('{', loc + 1);
    }
    flags.erase(std::remove(flags.begin(), flags.end(), '!'), flags.end());
}

/// Check if a string is a member of a list of strings and optionally ignore case or ignore underscores
inline std::ptrdiff_t find_member(std::string name,
                                  const std::vector<std::string> names,
                                  bool ignore_case = false,
                                  bool ignore_underscore = false) {
    auto it = std::end(names);
    if(ignore_case) {
        if(ignore_underscore) {
            name = detail::to_lower(detail::remove_underscore(name));
            it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
                return detail::to_lower(detail::remove_underscore(local_name)) == name;
            });
        } else {
            name = detail::to_lower(name);
            it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
                return detail::to_lower(local_name) == name;
            });
        }

    } else if(ignore_underscore) {
        name = detail::remove_underscore(name);
        it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
            return detail::remove_underscore(local_name) == name;
        });
    } else
        it = std::find(std::begin(names), std::end(names), name);

    return (it != std::end(names)) ? (it - std::begin(names)) : (-1);
}

/// Find a trigger string and call a modify callable function that takes the current string and starting position of the
/// trigger and returns the position in the string to search for the next trigger string
template <typename Callable> inline std::string find_and_modify(std::string str, std::string trigger, Callable modify) {
    size_t start_pos = 0;
    while((start_pos = str.find(trigger, start_pos)) != std::string::npos) {
        start_pos = modify(str, start_pos);
    }
    return str;
}

/// Split a string '"one two" "three"' into 'one two', 'three'
/// Quote characters can be ` ' or "
inline std::vector<std::string> split_up(std::string str) {

    const std::string delims("\'\"`");
    auto find_ws = [](char ch) { return std::isspace<char>(ch, std::locale()); };
    trim(str);

    std::vector<std::string> output;
    bool embeddedQuote = false;
    char keyChar = ' ';
    while(!str.empty()) {
        if(delims.find_first_of(str[0]) != std::string::npos) {
            keyChar = str[0];
            auto end = str.find_first_of(keyChar, 1);
            while((end != std::string::npos) && (str[end - 1] == '\\')) { // deal with escaped quotes
                end = str.find_first_of(keyChar, end + 1);
                embeddedQuote = true;
            }
            if(end != std::string::npos) {
                output.push_back(str.substr(1, end - 1));
                str = str.substr(end + 1);
            } else {
                output.push_back(str.substr(1));
                str = "";
            }
        } else {
            auto it = std::find_if(std::begin(str), std::end(str), find_ws);
            if(it != std::end(str)) {
                std::string value = std::string(str.begin(), it);
                output.push_back(value);
                str = std::string(it, str.end());
            } else {
                output.push_back(str);
                str = "";
            }
        }
        // transform any embedded quotes into the regular character
        if(embeddedQuote) {
            output.back() = find_and_replace(output.back(), std::string("\\") + keyChar, std::string(1, keyChar));
            embeddedQuote = false;
        }
        trim(str);
    }
    return output;
}

/// Add a leader to the beginning of all new lines (nothing is added
/// at the start of the first line). `"; "` would be for ini files
///
/// Can't use Regex, or this would be a subs.
inline std::string fix_newlines(std::string leader, std::string input) {
    std::string::size_type n = 0;
    while(n != std::string::npos && n < input.size()) {
        n = input.find('\n', n);
        if(n != std::string::npos) {
            input = input.substr(0, n + 1) + leader + input.substr(n + 1);
            n += leader.size();
        }
    }
    return input;
}

/// This function detects an equal or colon followed by an escaped quote after an argument
/// then modifies the string to replace the equality with a space.  This is needed
/// to allow the split up function to work properly and is intended to be used with the find_and_modify function
/// the return value is the offset+1 which is required by the find_and_modify function.
inline size_t escape_detect(std::string &str, size_t offset) {
    auto next = str[offset + 1];
    if((next == '\"') || (next == '\'') || (next == '`')) {
        auto astart = str.find_last_of("-/ \"\'`", offset - 1);
        if(astart != std::string::npos) {
            if(str[astart] == ((str[offset] == '=') ? '-' : '/'))
                str[offset] = ' '; // interpret this as a space so the split_up works properly
        }
    }
    return offset + 1;
}

/// Add quotes if the string contains spaces
inline std::string &add_quotes_if_needed(std::string &str) {
    if((str.front() != '"' && str.front() != '\'') || str.front() != str.back()) {
        char quote = str.find('"') < str.find('\'') ? '\'' : '"';
        if(str.find(' ') != std::string::npos) {
            str.insert(0, 1, quote);
            str.append(1, quote);
        }
    }
    return str;
}

} // namespace detail

} // namespace CLI
