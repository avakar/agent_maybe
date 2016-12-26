#ifndef ARGPARSE_HPP
#define ARGPARSE_HPP

#include <initializer_list>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string_view>

struct param_definition final
{
	enum class target_type { string, integer };

	struct target
	{
		target_type type;
		void * ptr;

		target(std::string & var)
			: type(target_type::string), ptr(&var)
		{
		}

		target(int & var)
			: type(target_type::integer), ptr(&var)
		{
		}
	};

	param_definition(target var, std::string_view long_opt);
	param_definition(target var, std::string_view long_opt, char short_opt, bool required = false, std::string_view metavar = "");
	param_definition(target var, std::string_view long_opt, bool required);


	target var_;
	std::string long_opt_;
	std::string metavar_;
	char short_opt_;
	bool required_;
};

std::string format_opt_help(std::string_view prog_name, param_definition const * first, param_definition const * last);
void print_opt_help(std::string_view prog_name, param_definition const * first, param_definition const * last);

void parse_argv(int argc, char * argv[], std::initializer_list<param_definition> params);
bool parse_args(int argc, char * argv[], std::initializer_list<param_definition> params);

#endif // ARGPARSE_HPP
