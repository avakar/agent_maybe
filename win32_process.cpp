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

bool process::poll()
{
	assert(pimpl_);

	auto h = reinterpret_cast<HANDLE>(pimpl_);
	return WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
}

int32_t process::exit_code() const
{
	assert(pimpl_);

	auto h = reinterpret_cast<HANDLE>(pimpl_);

	DWORD exit_code;
	GetExitCodeProcess(h, &exit_code);

	return exit_code;
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

void append_cmdline(std::string & cmdline, std::string_view arg)
{
	if (!cmdline.empty())
		cmdline.append(1, ' ');

	bool needs_quotes = false;
	for (char ch : arg)
	{
		if (ch == ' ' || ch == '^')
		{
			needs_quotes = true;
			break;
		}
	}

	if (!needs_quotes)
	{
		cmdline.append(arg);
		return;
	}

	cmdline.append(1, '"');

	auto append_chunk = [&cmdline](std::string_view chunk) {

		std::string_view slash_free = chunk;

		while (!slash_free.empty() && slash_free.back() == '\\')
			slash_free.remove_prefix(1);

		cmdline.append(chunk);
		cmdline.append(chunk.substr(slash_free.size()));
	};

	char const * first = arg.begin();
	char const * last = arg.end();
	for (char const * cur = first; cur != last; ++cur)
	{
		if (*cur != '"')
			continue;

		append_chunk({ first, cur });
		cmdline.append("\\\"");
		first = cur + 1;
	}

	append_chunk({ first, last });
	cmdline.append(1, '"');
}

int32_t run_process(std::string_view cmd)
{
	process p;
	p.start(cmd);
	return p.wait();
}
