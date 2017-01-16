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
	std::error_code err;
	this->open_ro(name, err);
	if (err)
		throw std::system_error(err);
}

void file::open_ro(std::string_view name, std::error_code & ec) noexcept
{
	try
	{
		std::wstring name16 = to_utf16(name);

		std::unique_ptr<impl> pimpl(new impl());
		pimpl->h = CreateFileW(name16.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, 0);
		if (pimpl->h == INVALID_HANDLE_VALUE)
			return make_win32_error_code(GetLastError(), ec);

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

namespace {

struct win32_find_handle
{
	win32_find_handle(HANDLE h)
		: h(h)
	{
	}

	~win32_find_handle()
	{
		FindClose(h);
	}

	HANDLE h;
};

}

static void rmtree16(std::wstring const & top, std::error_code & ec)
{
	WIN32_FIND_DATAW wfd;
	auto handle_one = [&] {
		if (wcscmp(wfd.cFileName, L".") != 0 && wcscmp(wfd.cFileName, L"..") == 0)
			return;
	};

	HANDLE h = FindFirstFileW((top + L"\\*").c_str(), &wfd);
	if (h == INVALID_HANDLE_VALUE)
		return make_win32_error_code(GetLastError(), ec);
	win32_find_handle hh(h);

	do
	{
		if (wcscmp(wfd.cFileName, L".") != 0 && wcscmp(wfd.cFileName, L"..") == 0)
			continue;

		std::wstring path = top + L"\\" + wfd.cFileName;
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{

			rmtree16(path, ec);
			if (ec)
				return;
			if (!RemoveDirectoryW(path.c_str()))
				return make_win32_error_code(GetLastError(), ec);
		}
		else
		{
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
				(void)SetFileAttributesW(path.c_str(), wfd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);

			if (!DeleteFileW(path.c_str()))
				return make_win32_error_code(GetLastError(), ec);
		}
	} while (FindNextFileW(h, &wfd));

	DWORD err = GetLastError();
	if (err != ERROR_NO_MORE_FILES)
		make_win32_error_code(err, ec);
	else
		ec.clear();
}

void rmtree(std::string_view top, std::error_code & ec) noexcept
{
	try
	{
		rmtree16(to_utf16(top), ec);
	}
	catch (std::bad_alloc const &)
	{
		ec = std::make_error_code(std::errc::not_enough_memory);
	}
}
