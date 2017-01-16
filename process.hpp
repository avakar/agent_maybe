#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string_view>

struct process
{
	process();
	process(process && o);
	process & operator=(process && o);
	~process();

	void close();

	void start(std::string_view cmd);

	bool poll();
	int32_t exit_code() const;

	int32_t wait();

private:
	struct impl;
	impl * pimpl_;
};

int32_t run_process(std::string_view cmd);

void append_cmdline(std::string & cmdline, std::string_view arg);

template <typename Range>
std::string make_cmdline(Range const & r)
{
	std::string cmdline;
	for (auto && e : r)
		append_cmdline(cmdline, e);
	return cmdline;
}

#endif // PROCESS_HPP
