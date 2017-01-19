#ifndef TLS_HPP
#define TLS_HPP

#include "stream.hpp"
#include <memory>

void tls_server(std::shared_ptr<istream> & in_tls, std::shared_ptr<ostream> & out_tls, istream & in, ostream & out, std::string const & key, std::string const & cert);

#endif // TLS_HPP
