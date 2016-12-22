#ifndef TAR_HPP
#define TAR_HPP

#include "stream.hpp"
#include "string_view.hpp"
#include <stdint.h>
#include <memory>

struct tarfile_writer
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

#endif // TAR_HPP
