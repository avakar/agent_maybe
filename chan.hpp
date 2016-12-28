#ifndef CHAN_HPP
#define CHAN_HPP

#include "stream.hpp"
#include <functional>
#include <memory>

std::shared_ptr<istream> make_istream(std::function<void(ostream & out)> fn);

#endif // CHAN_HPP
