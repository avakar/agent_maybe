#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string_view>
#include <vector>
#include <string>

struct process
{
	process();
	process(process && o);
	process & operator=(process && o);
	~process();

	void close();

	template <typename Range>
	void start(Range const & r);

	void start(std::vector<std::string> cmd);
	void start(std::string_view cmd);

	bool poll();
	int32_t exit_code() const;

	int32_t wait();

private:
	struct impl;
	impl * pimpl_;
};

int32_t run_process(std::string_view cmd);

template <typename Range>
void process::start(Range const & r)
{
	std::vector<std::string> args;
	for (auto && e: r)
		args.push_back(e);
	this->start(std::move(args));
}

#endif // PROCESS_HPP
