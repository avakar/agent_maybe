#ifndef STRING_VIEW_HPP
#define STRING_VIEW_HPP

#include <iosfwd>
#include <iterator>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <assert.h>

namespace std
{

template <typename T, typename Traits = char_traits<T>>
class basic_string_view
{
public:
	using traits_type = Traits;
	using value_type = T;
	using pointer = T *;
	using const_pointer = T const *;
	using reference = T &;
	using const_reference = T const &;
	using const_iterator = T const *;
	using iterator = const_iterator;
	using const_reverse_iterator = reverse_iterator<const_iterator>;
	using reverse_iterator = const_reverse_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	static constexpr size_t npos = size_t(-1);

	constexpr basic_string_view() noexcept;
	constexpr basic_string_view(T const * str);
	constexpr basic_string_view(T const * str, size_t len);
	constexpr basic_string_view(T const * first, T const * last);

	template <typename Alloc>
	basic_string_view(basic_string<T, Traits, Alloc> const & str);

	template <typename Alloc>
	operator basic_string<T, Traits, Alloc>() const;

	constexpr const_iterator begin() const noexcept;
	constexpr const_iterator end() const noexcept;
	constexpr const_iterator cbegin() const noexcept;
	constexpr const_iterator cend() const noexcept;
	constexpr const_reverse_iterator rbegin() const noexcept;
	constexpr const_reverse_iterator rend() const noexcept;
	constexpr const_reverse_iterator crbegin() const noexcept;
	constexpr const_reverse_iterator crend() const noexcept;

	constexpr size_t size() const noexcept;
	constexpr size_t length() const noexcept;
	constexpr size_t max_size() const noexcept;
	constexpr bool empty() const noexcept;

	constexpr const_reference operator[](size_t pos) const;
	constexpr const_reference at(size_t pos) const;
	constexpr const_reference front() const;
	constexpr const_reference back() const;
	constexpr const_pointer data() const noexcept;

	void remove_prefix(size_t n);
	void remove_suffix(size_t n);
	void swap(basic_string_view& s) noexcept;

	size_t copy(T * s, size_t n, size_t pos = 0) const;
	basic_string_view substr(size_t pos = 0, size_t n = npos) const;

	int compare(basic_string_view s) const noexcept;
	int compare(size_t pos1, size_t n1, basic_string_view s) const;
	int compare(size_t pos1, size_t n1, basic_string_view s,
		size_t pos2, size_t n2) const;
	int compare(T const * s) const;
	int compare(size_t pos1, size_t n1, T const * s) const;
	int compare(size_t pos1, size_t n1, T const * s,
		size_t n2) const;

	size_t find(basic_string_view s, size_t pos = 0) const noexcept;
	size_t find(T c, size_t pos = 0) const noexcept;
	size_t find(T const * s, size_t pos, size_t n) const;
	size_t find(T const * s, size_t pos = 0) const;

	friend bool operator==(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) == 0;
	}

	friend bool operator!=(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) != 0;
	}

	friend bool operator<(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) < 0;
	}

	friend bool operator>(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) > 0;
	}

	friend bool operator<=(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) <= 0;
	}

	friend bool operator>=(basic_string_view x, basic_string_view y) noexcept
	{
		return x.compare(y) >= 0;
	}

private:
	const_pointer first_;
	const_pointer last_;
};

template <typename T, typename Traits>
basic_ostream<T, Traits> & operator<<(basic_ostream<T, Traits>& os, basic_string_view<T, Traits> str);

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;

}

template <typename T, typename Traits>
constexpr std::basic_string_view<T, Traits>::basic_string_view() noexcept
	: first_(nullptr), last_(nullptr)
{
}

template <typename T, typename Traits>
constexpr std::basic_string_view<T, Traits>::basic_string_view(T const * str)
	: first_(str), last_(str + Traits::length(str))
{
}

template <typename T, typename Traits>
constexpr std::basic_string_view<T, Traits>::basic_string_view(T const * str, size_t len)
	: first_(str), last_(str + len)
{
}

template <typename T, typename Traits>
constexpr std::basic_string_view<T, Traits>::basic_string_view(T const * first, T const * last)
	: first_(first), last_(last)
{
}

template <typename T, typename Traits>
template <typename Alloc>
std::basic_string_view<T, Traits>::basic_string_view(basic_string<T, Traits, Alloc> const & str)
	: first_(str.data()), last_(first_ + str.size())
{
}

template <typename T, typename Traits>
template <typename Alloc>
std::basic_string_view<T, Traits>::operator std::basic_string<T, Traits, Alloc>() const
{
	return std::basic_string<T, Traits, Alloc>(first_, last_);
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::begin() const noexcept -> const_iterator
{
	return first_;
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::end() const noexcept -> const_iterator
{
	return last_;
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::cbegin() const noexcept -> const_iterator
{
	return first_;
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::cend() const noexcept -> const_iterator
{
	return last_;
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::rbegin() const noexcept -> const_reverse_iterator
{
	return std::make_reverse_iterator(last_);
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::rend() const noexcept -> const_reverse_iterator
{
	return std::make_reverse_iterator(first_);
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::crbegin() const noexcept -> const_reverse_iterator
{
	return std::make_reverse_iterator(last_);
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::crend() const noexcept -> const_reverse_iterator
{
	return std::make_reverse_iterator(first_);
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::size() const noexcept -> size_t
{
	return last_ - first_;
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::length() const noexcept -> size_t
{
	return this->size();
}

template <typename T, typename Traits>
constexpr auto std::basic_string_view<T, Traits>::max_size() const noexcept -> size_t
{
	return size_t(-1);
}

template <typename T, typename Traits>
constexpr bool std::basic_string_view<T, Traits>::empty() const noexcept
{
	return first_ == last_;
}

template <typename T, typename Traits>
constexpr T const & std::basic_string_view<T, Traits>::operator[](size_t pos) const
{
	assert(pos < this->size());
	return first_[pos];
}

template <typename T, typename Traits>
constexpr T const & std::basic_string_view<T, Traits>::at(size_t pos) const
{
	if (pos >= this->size())
		throw out_of_range("out of range");
	return first_[pos];
}

template <typename T, typename Traits>
constexpr T const & std::basic_string_view<T, Traits>::front() const
{
	assert(this->empty());
	return first_[0];
}

template <typename T, typename Traits>
constexpr T const & std::basic_string_view<T, Traits>::back() const
{
	assert(this->empty());
	return last_[-1];
}

template <typename T, typename Traits>
constexpr T const * std::basic_string_view<T, Traits>::data() const noexcept
{
	return first_;
}

template <typename T, typename Traits>
void std::basic_string_view<T, Traits>::remove_prefix(size_t n)
{
	assert(n <= this->size());
	first_ += n;
}

template <typename T, typename Traits>
void std::basic_string_view<T, Traits>::remove_suffix(size_t n)
{
	assert(n <= this->size());
	last_ -= n;
}

template <typename T, typename Traits>
void std::basic_string_view<T, Traits>::swap(basic_string_view & s) noexcept
{
	using std::swap;
	swap(first_, s.first_);
	swap(last_, s.last_);
}

template <typename T, typename Traits>
size_t std::basic_string_view<T, Traits>::copy(T * s, size_t n, size_t pos) const
{
	if (pos > this->size())
		throw out_of_range("out of range");

	size_t rlen = (std::min)(n, this->size() - pos);
	Traits::copy(s, first_ + pos, rlen);
	return rlen;
}

template <typename T, typename Traits>
auto std::basic_string_view<T, Traits>::substr(size_t pos, size_t n) const -> basic_string_view
{
	if (pos > this->size())
		throw out_of_range("out of range");

	size_t rlen = (std::min)(n, this->size() - pos);
	return basic_string_view(first_ + pos, rlen);
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(basic_string_view s) const noexcept
{
	size_t rlen = (std::min)(this->size(), s.size());
	int r = Traits::compare(first_, s.first_, rlen);
	if (r != 0)
		return r;

	if (this->size() < s.size())
		return -1;
	else if (this->size() == s.size())
		return 0;
	else
		return 1;
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(size_t pos1, size_t n1, basic_string_view s) const
{
	return this->substr(pos1, n1).compare(s);
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(size_t pos1, size_t n1, basic_string_view s, size_t pos2, size_t n2) const
{
	return this->substr(pos1, n1).compare(s.substr(pos2, n2));
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(T const * s) const
{
	return this->compare(basic_string_view(s));
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(size_t pos1, size_t n1, T const * s) const
{
	return this->substr(pos1, n1).compare(basic_string_view(s));
}

template <typename T, typename Traits>
int std::basic_string_view<T, Traits>::compare(size_t pos1, size_t n1, T const * s, size_t n2) const
{
	return this->substr(pos1, n1).compare(basic_string_view(s, n2));
}

template <typename T, typename Traits>
size_t std::basic_string_view<T, Traits>::find(basic_string_view s, size_t pos) const noexcept
{
	assert(pos < this->size());
	while (s.size() <= this->size() - pos)
	{
		if (this->substr(pos, s.size()) == s)
			return pos;
		++pos;
	}

	return npos;
}

template <typename T, typename Traits>
size_t std::basic_string_view<T, Traits>::find(T c, size_t pos) const noexcept
{
	return this->find(basic_string_view(&c, 1), pos);
}

template <typename T, typename Traits>
size_t std::basic_string_view<T, Traits>::find(T const * s, size_t pos, size_t n) const
{
	return this->find(basic_string_view(s, n), pos);
}

template <typename T, typename Traits>
size_t std::basic_string_view<T, Traits>::find(T const * s, size_t pos) const
{
	return this->find(basic_string_view(s), pos);
}

template <typename T, typename Traits>
std::basic_ostream<T, Traits> & std::operator<<(basic_ostream<T, Traits>& os, basic_string_view<T, Traits> str)
{
	os.write(str.data(), str.size());
	return os;
}

#endif // STRING_VIEW_HPP
