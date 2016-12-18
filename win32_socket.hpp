#ifndef WIN32_SOCKET_HPP
#define WIN32_SOCKET_HPP

#include "stream.hpp"
#include <functional>
#include <stdint.h>

void tcp_listen(uint16_t port, std::function<void(istream & in, ostream & out)> handler);

#endif // WIN32_SOCKET_HPP
