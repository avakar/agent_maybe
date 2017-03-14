#include "file.hpp"
#include <memory>
#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

struct file::impl final
	: istream, ostream
{
	int fd;

	size_t read(char * buf, size_t len) override
	{
		ssize_t r = ::read(fd, buf, len);
		if (r < 0)
			throw std::runtime_error("XXX");
		return r;
	}

	size_t write(char const * buf, size_t len) override
	{
		ssize_t r = ::write(fd, buf, len);
		if (r < 0)
			throw std::runtime_error("XXX");
		return r;
	}
};

file::file()
	: pimpl_(nullptr)
{
}

file::file(file && o)
	: pimpl_(o.pimpl_)
{
	o.pimpl_ = nullptr;
}

file::~file()
{
	this->close();
}

file & file::operator=(file && o)
{
	this->close();
	pimpl_ = o.pimpl_;
	o.pimpl_ = nullptr;
	return *this;
}

void file::open_ro(std::string_view name)
{
	std::error_code err;
	this->open_ro(name, err);
	if (err)
		throw std::system_error(err);
}

void file::open_ro(std::string_view name, std::error_code & ec) noexcept
{
	try
	{
		std::unique_ptr<impl> pimpl(new impl());

		pimpl->fd = open(std::string(name).c_str(), O_RDONLY);
		if (pimpl->fd < 0)
		{
			ec.assign(errno, std::system_category());
			return;
		}

		this->close();
		pimpl_ = pimpl.release();
		ec.clear();
	}
	catch (std::bad_alloc const &)
	{
		ec = std::make_error_code(std::errc::not_enough_memory);
	}
}


void file::create(std::string_view name)
{
	std::unique_ptr<impl> pimpl(new impl());
	pimpl->fd = open(std::string(name).c_str(), O_CREAT | O_RDWR, 0666);
	if (pimpl->fd < 0)
		throw std::system_error(errno, std::system_category());

	this->close();
	pimpl_ = pimpl.release();
}

void file::close()
{
	if (pimpl_ != nullptr)
	{
		::close(pimpl_->fd);
		delete pimpl_;
		pimpl_ = nullptr;
	}
}

uint64_t file::size()
{
	assert(pimpl_);

	struct stat st;
	fstat(pimpl_->fd, &st);
	return st.st_size;
}

uint64_t file::mtime()
{
	assert(pimpl_);

	struct stat st;
	fstat(pimpl_->fd, &st);
	return st.st_mtime;
}

istream & file::in_stream()
{
	return *pimpl_;
}

ostream & file::out_stream()
{
	return *pimpl_;
}

struct dir_guard
{
	DIR * dir;

	explicit dir_guard(DIR * dir)
		: dir(dir)
	{
	}

	~dir_guard()
	{
		closedir(dir);
	}
};

static void enum_files_impl(std::string const & top, DIR * dir, std::function<void(std::string_view fname)> const & cb)
{
	for (;;)
	{
		struct dirent * de = readdir(dir);
		if (!de)
			break;

		std::string n = top;
		n.append("/");
		n.append(de->d_name);

		DIR * subdir = opendir(n.c_str());
		if (!subdir)
		{
			if (errno == ENOTDIR)
				cb(n);
			throw std::system_error(errno, std::system_category());
		}

		dir_guard subdir_guard(subdir);
		enum_files_impl(n, subdir, cb);
	}
}

void enum_files(std::string_view top, std::function<void(std::string_view fname)> const & cb)
{
	std::string t(top);
	DIR * dir = opendir(t.c_str());
	if (!dir)
		throw std::system_error(errno, std::system_category());

	dir_guard dg(dir);
	enum_files_impl(t, dir, cb);
}

std::string join_paths(std::string_view lhs, std::string_view rhs)
{
	std::string r = lhs;
	r.append("/");
	r.append(rhs);
	return r;
}

static void rmtree_impl(std::string const & top, DIR * dir, std::error_code & ec) noexcept
{
	for (;;)
	{
		struct dirent * de = readdir(dir);
		if (!de)
			return;

		std::string n = top;
		n.append("/");
		n.append(de->d_name);

		DIR * subdir = opendir(n.c_str());
		if (!subdir)
		{
			if (errno == ENOTDIR)
				unlink(n.c_str());
			else
				return ec.assign(errno, std::system_category());
		}

		dir_guard subdir_guard(subdir);
		rmtree_impl(n, subdir, ec);
		if (ec)
			return;

		::rmdir(top.c_str());
	}
}

void rmtree(std::string_view top, std::error_code & ec) noexcept
{
	std::string t(top);

	DIR * dir = opendir(t.c_str());
	if (!dir)
		return ec.assign(errno, std::system_category());

	dir_guard dg(dir);

	std::error_code ec2;
	rmtree_impl(t, dir, ec2);

	if (!ec2)
		rmdir(t.c_str());

	ec = ec2;
}
