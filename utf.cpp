#include "utf.hpp"
#include "win32_error.hpp"
#include <windows.h>

std::wstring to_utf16(std::string_view s)
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

std::string to_utf8(std::wstring_view s)
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
