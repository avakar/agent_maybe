#ifndef TLS_HPP
#define TLS_HPP

#include "stream.hpp"
#include <memory>
#include <vector>
#include <string_view>

std::string tls_server(std::shared_ptr<istream> & in_tls, std::shared_ptr<ostream> & out_tls, istream & in, ostream & out, std::string const & key, std::string const & cert, std::vector<std::string_view> const & protos = {});

#endif // TLS_HPP
