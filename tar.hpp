#ifndef TAR_HPP
#define TAR_HPP

#include "stream.hpp"
#include "string_view.hpp"
#include <stdint.h>

struct tarfile_writer
{
	explicit tarfile_writer(ostream & out);
	void add(std::string_view name, uint64_t size, uint64_t mtime, istream & file);
	void close();

private:
	ostream & out_;
};

#endif // TAR_HPP
