#include "tar.hpp"
#include <algorithm>
#include <numeric>
#include <stdint.h>
#include "zlib.h"

static void write_oct(char * buf, size_t len, uint64_t num)
{
	buf[--len] = ' ';

	while (len)
	{
		buf[--len] = '0' + (num & 0x7);
		num >>= 3;
	}
}

static uint64_t load_oct(char const * buf, size_t len)
{
	auto is_oct = [](char ch) {
		return '0' <= ch && ch <= '7';
	};

	if (!is_oct(buf[0]))
		throw std::runtime_error("not a number");

	uint64_t r = 0;
	for (; len != 0; --len)
	{
		if (*buf == ' ' || *buf == 0)
			break;

		if (!is_oct(*buf))
			throw std::runtime_error("not a number");

		r = r * 8 + (*buf++ - '0');
	}

	return r;
}

static char const g_empty_two_blocks[1024] = {};

tarfile_writer::tarfile_writer(ostream & out)
	: out_(out)
{
}

void tarfile_writer::add(std::string_view name, uint64_t size, uint64_t mtime, istream & file)
{
	char buf[16 * 1024] = {};

	if (name.size() > 100)
		throw std::runtime_error("tar name too long");

	// name
	memcpy(buf, name.data(), name.size());

	// mode
	memcpy(buf + 100, "000666 ", 7);

	// uid, gid
	memcpy(buf + 108, "000000 ", 7);
	memcpy(buf + 116, "000000 ", 7);

	// size
	write_oct(buf + 124, 12, size);

	// mtime
	write_oct(buf + 136, 12, mtime);

	// typeflag
	buf[156] = '0';

	// magic+version
	memcpy(buf + 257, "ustar\x0000", 8);

	// chksum
	write_oct(buf + 148, 8, std::accumulate(buf, buf + 512, (size_t)(0x20 * 8)));

	out_.write_all(buf, 512);

	size_t padding = 512 - (size % 512);
	if (padding == 512)
		padding = 0;

	while (size)
	{
		size_t chunk = sizeof buf;
		if (chunk > size)
			chunk = (size_t)size;
		size_t r = file.read(buf, chunk);
		assert(r != 0);

		size -= chunk;
		out_.write_all(buf, chunk);
	}

	if (padding)
	{
		memset(buf, 0, padding);
		out_.write_all(buf, padding);
	}
}

void tarfile_writer::close()
{
	out_.write_all(g_empty_two_blocks, sizeof g_empty_two_blocks);
	out_.close();
}

tarfile_reader::tarfile_reader(istream & in)
	: in_(in), cur_len_(0), next_header_offset_(0)
{
}

bool tarfile_reader::next(std::string & name, uint64_t & size, std::shared_ptr<istream> & content)
{
	char header[32*1024];
	for (;;)
	{
		while (next_header_offset_ != 0)
		{
			size_t chunk = sizeof header;
			if (chunk > next_header_offset_)
				chunk = (size_t)next_header_offset_;
			size_t r = in_.read(header, chunk);
			if (r == 0)
				throw std::runtime_error("premature end of stream");
			next_header_offset_ -= r;
		}

		in_.read_all(header, 512);

		if (memcmp(header, g_empty_two_blocks, 512) == 0)
			return false;

		if (memcmp(header + 257, "ustar", 6) != 0)
			throw std::runtime_error("invalid tar");

		uint64_t chksum = load_oct(header + 148, 8);
		memset(header + 148, ' ', 8);

		uint64_t chk = std::accumulate(header, header + 512, 0);
		if (chk != chksum)
			throw std::runtime_error("invalid checksum");

		size_t prefix_len = 0;
		while (prefix_len < 155 && header[345 + prefix_len] != 0)
			++prefix_len;

		size_t name_len = 0;
		while (name_len < 100 && header[name_len] != 0)
			++name_len;

		if (prefix_len != 0)
		{
			name.assign(header + 345, prefix_len);
			name.append("/");
		}
		else
		{
			name.clear();
		}

		name.append(header, name_len);
		size = load_oct(header + 124, 12);
		cur_len_ = size;
		next_header_offset_ = (cur_len_ + 511) & ~(uint64_t)(0x1ff);
		content = std::shared_ptr<istream>(std::shared_ptr<istream>(), this);
		return true;
	}
}

size_t tarfile_reader::read(char * buf, size_t len)
{
	if (cur_len_ < len)
		len = (size_t)cur_len_;

	if (len == 0)
		return 0;

	size_t r = in_.read(buf, len);
	cur_len_ -= r;
	next_header_offset_ -= r;

	if (r == 0 && cur_len_ != 0)
		throw std::runtime_error("premature end of stream");

	return r;
}

struct gzip_filter::impl
{
	bool compress;
	z_stream z;

	void close()
	{
		if (compress)
			deflateEnd(&z);
		else
			inflateEnd(&z);
	}
};

gzip_filter::gzip_filter(bool compress)
	: pimpl_(new impl{ compress })
{
	pimpl_->z.zalloc = nullptr;
	pimpl_->z.zfree = nullptr;
	pimpl_->z.opaque = nullptr;
	pimpl_->z.next_in = nullptr;

	int r;
	if (compress)
		r = deflateInit2(&pimpl_->z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	else
		r = inflateInit2(&pimpl_->z, 15 + 16);

	if (r < 0)
	{
		delete pimpl_;
		throw std::runtime_error("cannot initialize zlib");
	}
}

gzip_filter::gzip_filter(gzip_filter && o)
	: pimpl_(o.pimpl_)
{
	o.pimpl_ = nullptr;
}

gzip_filter::~gzip_filter()
{
	if (pimpl_)
	{
		pimpl_->close();
		delete pimpl_;
	}
}

gzip_filter & gzip_filter::operator=(gzip_filter && o)
{
	std::swap(pimpl_, o.pimpl_);
	return *this;
}

std::pair<size_t, size_t> gzip_filter::process(char const * inbuf, size_t inlen, char * outbuf, size_t outlen)
{
	assert(pimpl_ != nullptr);

	pimpl_->z.next_in = (Bytef *)inbuf;
	pimpl_->z.avail_in = inlen;

	pimpl_->z.next_out = (Bytef *)outbuf;
	pimpl_->z.avail_out = outlen;

	int r;
	if (pimpl_->compress)
		r = deflate(&pimpl_->z, Z_NO_FLUSH);
	else
		r = inflate(&pimpl_->z, Z_NO_FLUSH);

	if (r < 0)
		throw std::runtime_error("zlib error");

	return std::make_pair(pimpl_->z.next_in - (Bytef *)inbuf, pimpl_->z.next_out - (Bytef *)outbuf);
}

size_t gzip_filter::finish(char * outbuf, size_t outlen)
{
	if (pimpl_ == nullptr)
		return 0;

	pimpl_->z.next_in = nullptr;
	pimpl_->z.avail_in = 0;

	pimpl_->z.next_out = (Bytef *)outbuf;
	pimpl_->z.avail_out = outlen;

	size_t ret = 0;
	while (ret == 0)
	{
		int r;
		if (pimpl_->compress)
			r = deflate(&pimpl_->z, Z_FINISH);
		else
			r = inflate(&pimpl_->z, Z_FINISH);

		if (r < 0)
			throw std::runtime_error("zlib error");

		ret = pimpl_->z.next_out - (Bytef *)outbuf;

		if (r == Z_STREAM_END)
		{
			pimpl_->close();
			delete pimpl_;
			pimpl_ = nullptr;
			break;
		}
	}

	return ret;
}
