#include "process.hpp"
#include <sys/types.h>
#include <unistd.h>

process::process()
	: pimpl_(nullptr)
{
}

process::process(process && o)
	: pimpl_(o.pimpl_)
{
	o.pimpl_ = nullptr;
}

process & process::operator=(process && o)
{
	using std::swap;
	swap(pimpl_, o.pimpl_);
	return *this;
}

process::~process()
{
	this->close();
}

void process::close()
{
	pimpl_ = nullptr;
}

void process::start(std::vector<std::string> args)
{
	std::vector<char const *> arg_ptrs;
	for (std::string const & arg: args)
		arg_ptrs.push_back(arg.c_str());
	arg_ptrs.push_back(nullptr);

	pid_t pid = vfork();
	if (pid < 0)
		throw std::system_error(errno, std::system_category());

	if (pid == 0)
	{
		execvp(arg_ptrs[0], (char **)arg_ptrs.data());
		_exit(errno);
	}

	this->close();
	pimpl_ = reinterpret_cast<impl *>(pid);
}

void process::start(std::string_view args)
{
	// XXX
}

bool process::poll()
{
	assert(pimpl_);

	// XXX
	return false;
}

int32_t process::exit_code() const
{
	assert(pimpl_);

	// XXX
	return 0;
}

int32_t process::wait()
{
	assert(pimpl_);

	// XXX
	return 0;
}

int32_t run_process(std::string_view cmd)
{
	process p;
	p.start(cmd);
	return p.wait();
}
