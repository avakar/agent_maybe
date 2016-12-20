#ifndef STREAM_HPP
#define STREAM_HPP

#include "string_view.hpp"
#include <assert.h>

struct istream
{
	virtual size_t read(char * buf, size_t len) = 0;
};

struct ostream
{
	virtual size_t write(char const * buf, size_t len) = 0;

	void write_all(char const * buf, size_t len)
	{
		while (len)
		{
			size_t r = this->write(buf, len);
			assert(r != 0);
			assert(r <= len);

			buf += r;
			len -= r;
		}
	}
};

struct string_istream
	: istream
{
	explicit string_istream(std::string_view str)
		: str_(str)
	{
	}

	size_t read(char * buf, size_t len) override
	{
		if (len > str_.size())
			len = str_.size();
		memcpy(buf, str_.data(), len);
		return len;
	}

private:
	std::string_view str_;
};

#endif // STREAM_HPP