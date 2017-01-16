#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <string_view>
#include <string>

template <typename... P>
std::string format(std::string_view fmt, P &&... p);

#include "format_impl.hpp"

#endif // FORMAT_HPP
