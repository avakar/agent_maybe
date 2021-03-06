#ifndef WIN32_ERROR_HPP
#define WIN32_ERROR_HPP

#include <windows.h>
#include <stdexcept>
#include <system_error>

struct win32_error
	: std::runtime_error
{
	explicit win32_error(DWORD err)
		: std::runtime_error("win32_error"), err_(err)
	{
	}

	DWORD err_;
};

void make_win32_error_code(DWORD err, std::error_code & ec);

#endif // WIN32_ERROR_HPP
