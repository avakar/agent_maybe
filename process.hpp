#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "string_view.hpp"

struct process
{
	process();
	process(process && o);
	process & operator=(process && o);
	~process();

	void close();

	void start(std::string_view cmd);
	int32_t wait();

private:
	struct impl;
	impl * pimpl_;
};

int32_t run_process(std::string_view cmd);

#endif // PROCESS_HPP
