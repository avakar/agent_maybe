#include "file.hpp"
#include "utf.hpp"
#include <memory>
#include "win32_error.hpp"
#include <windows.h>
#include <stdexcept>

struct file::impl final
	: istream, ostream
{
	HANDLE h;

	size_t read(char * buf, size_t len) override
	{
		if (len > MAXDWORD)
			len = MAXDWORD;

		DWORD dwRead;
		ReadFile(h, buf, (DWORD)len, &dwRead, nullptr);

		return dwRead;
	}

	size_t write(char const * buf, size_t len) override
	{
		if (len > MAXDWORD)
			len = MAXDWORD;

		DWORD dwWritten;
		WriteFile(h, buf, (DWORD)len, &dwWritten, nullptr);

		return dwWritten;
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
	std::wstring name16 = to_utf16(name);

	std::unique_ptr<impl> pimpl(new impl());
	pimpl->h = CreateFileW(name16.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, 0);
	if (pimpl->h == INVALID_HANDLE_VALUE)
		throw win32_error(GetLastError());

	this->close();
	pimpl_ = pimpl.release();
}

void file::create(std::string_view name)
{
	std::wstring name16 = to_utf16(name);

	std::unique_ptr<impl> pimpl(new impl());
	pimpl->h = CreateFileW(name16.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, 0);
	if (pimpl->h == INVALID_HANDLE_VALUE)
		throw win32_error(GetLastError());

	this->close();
	pimpl_ = pimpl.release();
}

void file::close()
{
	if (pimpl_ != nullptr)
	{
		CloseHandle(pimpl_->h);
		delete pimpl_;
		pimpl_ = nullptr;
	}
}

uint64_t file::size()
{
	assert(pimpl_);

	LARGE_INTEGER li;
	if (!GetFileSizeEx(pimpl_->h, &li))
		throw win32_error(GetLastError());

	return li.QuadPart;
}

uint64_t file::mtime()
{
	assert(pimpl_);

	FILETIME ft;
	if (!GetFileTime(pimpl_->h, nullptr, nullptr, &ft))
		throw win32_error(GetLastError());

	return (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ull - 11644473600ull;
}

istream & file::in_stream()
{
	return *pimpl_;
}

ostream & file::out_stream()
{
	return *pimpl_;
}

static void enum_files16(std::wstring & top, size_t prefix_len, std::function<void(std::string_view fname)> const & cb)
{
	std::wstring path = 
	top.append(L"\\*");

	WIN32_FIND_DATAW wfd;
	HANDLE h = FindFirstFileW(top.c_str(), &wfd);
	top.resize(top.size() - 2);

	if (h == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND)
			return;
		throw win32_error(err);
	}

	do
	{
		if (wcscmp(wfd.cFileName, L".") == 0
			|| wcscmp(wfd.cFileName, L"..") == 0)
		{
			continue;
		}

		size_t prev_len = top.size();
		top.append(L"\\");
		top.append(wfd.cFileName);

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			enum_files16(top, prefix_len, cb);
		}
		else
		{
			cb(to_utf8(top.substr(prefix_len + 1)));
		}

		top.resize(prev_len);

	} while (FindNextFileW(h, &wfd));

	FindClose(h);
}

void enum_files(std::string_view top, std::function<void(std::string_view fname)> const & cb)
{
	auto top16 = to_utf16(top);
	enum_files16(top16, top16.size(), cb);
}

std::string join_paths(std::string_view lhs, std::string_view rhs)
{
	std::string r = lhs;
	r.append("\\");
	r.append(rhs);
	return r;
}
