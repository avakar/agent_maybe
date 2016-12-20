#include "tar.hpp"
#include <algorithm>
#include <numeric>

static void write_oct(char * buf, size_t len, uint64_t num)
{
	buf[--len] = ' ';

	while (len)
	{
		buf[--len] = '0' + (num & 0x7);
		num >>= 3;
	}
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
		size_t chunk = (std::min)(size, sizeof buf);
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
}
