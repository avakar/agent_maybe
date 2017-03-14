#include "guid.hpp"

#ifdef WIN32

#include <system_error>
#include <windows.h>

std::string new_uuid()
{
	GUID guid;
	HRESULT hr = CoCreateGuid(&guid);
	if (FAILED(hr))
		throw std::system_error(hr, std::system_category());

	char r[37];

	snprintf(r, sizeof r, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x%02x%02x",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7], guid.Data4[8], guid.Data4[9]);
	
	return r;
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

std::string new_uuid()
{
	int fd = open("/proc/sys/kernel/random/uuid", O_RDONLY);

	char r[36];
	read(fd, r, sizeof r);

	close(fd);

	return std::string(r, sizeof r);
}


#endif
