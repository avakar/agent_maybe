#ifndef CHAN_HPP
#define CHAN_HPP

#include "stream.hpp"
#include <functional>

struct chan
	: istream, ostream
{
	chan();
	chan(chan && o);
	~chan();
	chan & operator=(chan && o);

	size_t read(char * buf, size_t len) override;
	size_t write(char const * buf, size_t len) override;

	void add_coroutine(std::function<void()> co);

private:
	struct impl;
	impl * pimpl_;
};

#endif // CHAN_HPP
