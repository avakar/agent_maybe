#include "file.hpp"
#include <memory>
#include "win32_error.hpp"
#include <windows.h>
#include <stdexcept>

static std::wstring to_utf16(std::string_view s)
{
	std::wstring r;

	int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	if (len == 0)
		throw win32_error(GetLastError());

	r.resize(len + 1);

	len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &r[0], (int)r.size());
	if (len == 0)
		throw win32_error(GetLastError());

	assert(len < r.size());

	r.resize(len);
	return r;
}

static std::string to_utf8(std::wstring_view s)
{
	std::string r;

	int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), s.size(), nullptr, 0, nullptr, nullptr);
	if (len == 0)
		throw win32_error(GetLastError());

	r.resize(len + 1);

	len = WideCharToMultiByte(CP_UTF8, 0, s.data(), s.size(), &r[0], r.size(), nullptr, nullptr);
	if (len == 0)
		throw win32_error(GetLastError());

	assert(len < r.size());

	r.resize(len);
	return r;
}

struct file::impl final
	: istream
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
