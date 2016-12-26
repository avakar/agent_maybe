#ifndef FILE_HPP
#define FILE_HPP

#include "stream.hpp"
#include <string_view>
#include <functional>

struct file
{
	file();
	file(file && o);
	~file();
	file & operator=(file && o);

	void create(std::string_view name);
	void open_ro(std::string_view name);
	void close();

	uint64_t size();
	uint64_t mtime();

	istream & in_stream();
	ostream & out_stream();

private:
	struct impl;
	impl * pimpl_;
};

std::string join_paths(std::string_view lhs, std::string_view rhs);

void enum_files(std::string_view top, std::function<void(std::string_view fname)> const & cb);

#endif // FILE_HPP
