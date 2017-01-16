#include "win32_error.hpp"

void make_win32_error_code(DWORD err, std::error_code & ec)
{
	if (err == 0)
	{
		ec.clear();
		return;
	}

	ec.assign(err, std::system_category());
}
