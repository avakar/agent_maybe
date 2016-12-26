#ifndef UTF_HPP
#define UTF_HPP

#include <string_view>
#include <string>

std::string to_utf8(std::wstring_view s);
std::wstring to_utf16(std::string_view s);

#endif // UTF_HPP
