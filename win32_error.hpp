#ifndef WIN32_ERROR_HPP
#define WIN32_ERROR_HPP

#include <windows.h>

struct win32_error
	: std::runtime_error
{
	explicit win32_error(DWORD err)
		: std::runtime_error("win32_error"), err_(err)
	{
	}

	DWORD err_;
};

#endif // WIN32_ERROR_HPP
