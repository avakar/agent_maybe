#ifndef TAR_HPP
#define TAR_HPP

#include "stream.hpp"
#include <string_view>
#include <stdint.h>
#include <memory>

struct tarfile_writer final
{
	explicit tarfile_writer(ostream & out);
	void add(std::string_view name, uint64_t size, uint64_t mtime, istream & file);
	void close();

private:
	ostream & out_;
};

struct tarfile_reader final
	: private istream
{
	explicit tarfile_reader(istream & in);
	bool next(std::string & name, uint64_t & size, std::shared_ptr<istream> & content);

private:
	size_t read(char * buf, size_t len) override;

	istream & in_;
	uint64_t cur_len_;
	uint64_t next_header_offset_;
};

struct gzip_filter
{
	explicit gzip_filter(bool compress);
	gzip_filter(gzip_filter && o);
	~gzip_filter();
	gzip_filter & operator=(gzip_filter && o);

	std::pair<size_t, size_t> process(char const * inbuf, size_t inlen, char * outbuf, size_t outlen);
	size_t finish(char * outbuf, size_t outlen);

private:
	struct impl;
	impl * pimpl_;
};

template <typename Filter>
struct filter_writer final
	: ostream
{
	template <typename... P>
	filter_writer(ostream & out, P &&... p)
		: out_(out), filter_(std::forward<P>(p)...)
	{
	}

	size_t write(char const * buf, size_t len) override
	{
		char outbuf[8*1024];

		for (;;)
		{
			auto r = filter_.process(buf, len, outbuf, sizeof outbuf);
			out_.write_all(outbuf, r.second);

			if (r.first)
				return r.first;
		}
	}

	void close() override
	{
		char outbuf[8*1024];

		for (;;)
		{
			size_t r = filter_.finish(outbuf, sizeof outbuf);
			if (r == 0)
				break;
			out_.write_all(outbuf, r);
		}

		out_.close();
	}

private:
	ostream & out_;
	Filter filter_;
};

template <typename Filter>
struct filter_reader final
	: istream
{
	template <typename... P>
	filter_reader(istream & in, P &&... p)
		: in_(in), filter_(std::forward<P>(p)...), inptr_(inbuf_), inlen_(0)
	{
	}

	size_t read(char * buf, size_t len) override
	{
		if (inptr_ == nullptr)
			return filter_.finish(buf, len);

		for (;;)
		{
			if (inlen_ == 0)
			{
				inlen_ = in_.read(inbuf_, sizeof inbuf_);
				inptr_ = inlen_ == 0 ? nullptr : inbuf_;
			}

			if (inptr_ == nullptr)
				return filter_.finish(buf, len);

			auto r = filter_.process(inptr_, inlen_, buf, len);
			inptr_ += r.first;
			inlen_ -= r.first;

			if (r.second != 0)
				return r.second;
		}
	}

private:
	istream & in_;
	Filter filter_;

	char inbuf_[8 * 1024];
	char * inptr_;
	size_t inlen_;
};

using gzip_writer = filter_writer<gzip_filter>;
using gzip_reader = filter_reader<gzip_filter>;

#endif // TAR_HPP
