#include <type_traits>

inline void format_append(std::string & r, std::string_view fmt)
{
	r.append(fmt);
}

inline void format_append_one(std::string & r, std::string_view v)
{
	r.append(v);
}

template <typename Integral>
std::enable_if_t<std::is_integral<Integral>::value, void> format_append_one(std::string & r, Integral v)
{
	r.append(std::to_string(v));
}

template <typename P0, typename... P>
void format_append(std::string & r, std::string_view fmt, P0 && p0, P &&... p)
{
	char const * first = fmt.begin();
	char const * last = fmt.end();

	for (char const * cur = first; cur != last; ++cur)
	{
		if (*cur == '{' && cur != last && cur[1] == '}')
		{
			r.append(first, cur);
			format_append_one(r, std::forward<P0>(p0));
			return format_append(r, std::string_view(cur + 2, last), std::forward<P>(p)...);
		}
	}

	r.append(first, last);
}


template <typename... P>
std::string format(std::string_view fmt, P &&... p)
{
	std::string r;
	format_append(r, fmt, std::forward<P>(p)...);
	return r;
}
