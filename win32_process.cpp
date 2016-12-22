#include "process.hpp"
#include "utf.hpp"
#include "win32_error.hpp"
#include <windows.h>

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
	auto h = reinterpret_cast<HANDLE>(pimpl_);
	CloseHandle(h);
	pimpl_ = nullptr;
}

void process::start(std::string_view cmd)
{
	std::wstring cmd16 = to_utf16(cmd);

	STARTUPINFOW si = { sizeof si };
	PROCESS_INFORMATION pi;

	if (!CreateProcessW(nullptr, &cmd16[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
		throw win32_error(GetLastError());

	CloseHandle(pi.hThread);

	this->close();
	pimpl_ = reinterpret_cast<impl *>(pi.hProcess);
}

int32_t process::wait()
{
	assert(pimpl_);

	auto h = reinterpret_cast<HANDLE>(pimpl_);
	WaitForSingleObject(h, INFINITE);

	DWORD exit_code;
	GetExitCodeProcess(h, &exit_code);

	return exit_code;
}

int32_t run_process(std::string_view cmd)
{
	process p;
	p.start(cmd);
	return p.wait();
}
