#include "argparse.hpp"
#include <string_utils.hpp>
#include <list>
#include <sstream>
#include <iostream>

namespace {

struct arg_list
{
	arg_list(char const * const * first, char const * const * last)
		: first_(first), last_(last)
	{
	}

	bool empty() const
	{
		return first_ == last_;
	}

	std::string_view peek()
	{
		assert(first_ != last_);
		return *first_;
	}

	std::string_view pop()
	{
		assert(first_ != last_);
		return *first_++;
	}

private:
	char const * const * first_;
	char const * const * last_;
};

}

param_definition::param_definition(target var, std::string_view long_opt)
	: var_(var), long_opt_(long_opt), short_opt_(0)
{
	assert(!long_opt_.empty());
	assert(long_opt_[0] != '-' || (long_opt_.size() >= 2 && long_opt_[1] == '-'));

	if (starts_with(long_opt_, "--"))
	{
		metavar_ = long_opt_.substr(2);
		required_ = false;
	}
	else
	{
		metavar_ = long_opt_;
		required_ = true;
	}
}

param_definition::param_definition(target var, std::string_view long_opt, char short_opt, bool required, std::string_view metavar)
	: var_(var), long_opt_(long_opt), short_opt_(short_opt), required_(required), metavar_(metavar)
{
	assert(starts_with(long_opt_, "--"));

	if (metavar_.empty())
		metavar_ = long_opt_.substr(2);
}

param_definition::param_definition(target var, std::string_view long_opt, bool required)
	: param_definition(var, long_opt)
{
	required_ = required;
}

void parse_argv(int argc, char * argv[], std::initializer_list<param_definition> params)
{
	if (!parse_args(argc, argv, params))
		print_opt_help(argv[0], params.begin(), params.end());
}


bool parse_args(int argc, char * argv[], std::initializer_list<param_definition> params)
{
	arg_list args(argv + 1, argv + argc);

	std::list<param_definition const *> pos_defs;
	std::list<param_definition const *> opt_defs;
	for (auto && def : params)
	{
		if (starts_with(def.long_opt_, "--"))
			opt_defs.push_back(&def);
		else
			pos_defs.push_back(&def);
	}

	enum class action { keep, remove };

	auto store = [](param_definition::target const & tgt, std::string_view val) -> action {
		switch (tgt.type)
		{
		case param_definition::target_type::string:
			*static_cast<std::string *>(tgt.ptr) = val;
			break;
		case param_definition::target_type::integer:
			*static_cast<int *>(tgt.ptr) = std::stoi(val);
			break;
		}

		return action::remove;
	};

	auto pos_arg = [&](std::string_view arg) -> bool {
		if (pos_defs.empty())
			return false;

		action act = store(pos_defs.front()->var_, arg);
		if (act == action::remove)
			pos_defs.pop_front();
		return true;
	};

	auto opt_arg = [&](std::string_view opt) {
		auto it = opt_defs.begin();

		if (opt.size() == 1)
		{
			for (; it != opt_defs.end(); ++it)
			{
				if ((*it)->short_opt_ == opt[0])
					break;
			}
		}
		else
		{
			assert(starts_with(opt, "--"));
			for (; it != opt_defs.end(); ++it)
			{
				if ((*it)->long_opt_ == opt)
					break;
			}
		}

		if (it == opt_defs.end())
			return false;

		action act = store((*it)->var_, args.pop());
		if (act == action::remove)
			opt_defs.erase(it);
		return true;
	};

	bool ignore_options = false;
	while (!args.empty())
	{
		std::string_view arg = args.pop();
		if (ignore_options)
		{
			if (!pos_arg(arg))
				return false;
			continue;
		}

		if (arg == "--")
		{
			ignore_options = true;
			break;
		}

		if (arg.size() >= 2 && arg[0] == '-')
		{
			if (arg[1] == '-')
			{
				if (!opt_arg(arg))
					return false;
			}
			else
			{
				for (char c : arg.substr(1))
				{
					if (!opt_arg(std::string_view(&c, 1)))
						return false;
				}
			}
		}
		else
		{
			if (!pos_arg(arg))
				return false;
		}
	}

	for (auto && def : pos_defs)
	{
		if (def->required_)
			return false;
	}

	for (auto && def : opt_defs)
	{
		if (def->required_)
			return false;
	}

	return true;
}

std::string format_opt_help(std::string_view prog_name, param_definition const * first, param_definition const * last)
{
	char const * cur = prog_name.data() + prog_name.size();
	for (; cur != prog_name.data(); --cur)
	{
		if (cur[-1] == '\\' || cur[-1] == '/')
		{
			prog_name = cur;
			break;
		}
	}

	std::ostringstream ss;
	ss << "Usage: " << prog_name;
	for (; first != last; ++first)
	{
		param_definition const & param = *first;

		ss << " ";

		if (!param.required_)
			ss << "[";

		if (param.short_opt_)
			ss << "-" << param.short_opt_ << " ";
		else if (starts_with(param.long_opt_, "--"))
			ss << param.long_opt_ << " ";

		ss << param.metavar_;

		if (!param.required_)
			ss << "]";
	}

	ss << "\n";
	return ss.str();
}

void print_opt_help(std::string_view prog_name, param_definition const * first, param_definition const * last)
{
	std::cerr << format_opt_help(prog_name, first, last);
	exit(2);
}

