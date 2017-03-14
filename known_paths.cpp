#include "known_paths.hpp"

#ifdef WIN32

#include "utf.hpp"
#include <windows.h>
#include <shlobj.h>

namespace {

struct task_mem
{
	task_mem(void * mem)
		: mem(mem)
	{
	}

	~task_mem()
	{
		CoTaskMemFree(mem);
	}

	void * mem;
};

}

std::string get_appdata_dir()
{
	PWSTR path;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
	if (FAILED(hr))
		throw std::system_error(hr, std::system_category());

	task_mem path_tm(path);
	return to_utf8(path);
}

#else

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

std::string get_appdata_dir()
{
	char * home = getenv("HOME");
	if (!home)
	{
		passwd * pw = getpwuid(getuid());
		home = pw->pw_dir;
	}

	std::string r = home;
	r += "/.local/share";
	return r;
}

#endif
