#include "http_server.hpp"
#include "utils.hpp"
#include <algorithm>
#include <iostream>

static std::pair<uint16_t, std::string_view> const status_texts[] = {
	{ 200, "OK" },
	{ 303, "See Other" },
	{ 404, "Not Found" },
};

int compare_header_name(std::string_view const & lhs, std::string_view const & rhs) noexcept
{
	char const * lhs_first = lhs.data();
	char const * lhs_last = lhs_first + lhs.size();
	char const * rhs_first = rhs.data();
	char const * rhs_last = rhs_first + rhs.size();

	while (lhs_first != lhs_last && rhs_first != rhs_last)
	{
		auto l = *lhs_first++;
		auto r = *rhs_first++;

		if ('a' <= l && l <= 'z' && 'A' <= r && r <= 'Z')
			l -= ('a' - 'A');
		if ('a' <= r && r <= 'Z' && 'A' <= l && l <= 'Z')
			r -= ('a' - 'A');

		if (l != r)
			return l - r;
	}

	if (lhs_first != lhs_last)
		return 1;
	if (rhs_first != rhs_last)
		return -1;
	return 0;
}

std::pair<header_view const *, header_view const *> get_header_range(header_list const & headers, std::string_view name)
{
	auto r = std::equal_range(headers.begin(), headers.end(), name);
	return std::make_pair(&*r.first, &*r.second);
}

std::string_view const * get_single(header_list const & headers, std::string_view name)
{
	auto r = get_header_range(headers, name);
	if (r.first == r.second || std::next(r.first) != r.second)
		return nullptr;

	return &r.first->value;
}

namespace {

struct fixed_req_stream final
	: istream
{
	fixed_req_stream(std::string_view & prebuf, istream & in, uint64_t limit)
		: prebuf_(prebuf), in_(in), limit_(limit)
	{
	}

	size_t read(char * buf, size_t len) override
	{
		if (len > limit_)
			len = (size_t)limit_;
		if (len == 0)
			return 0;

		if (!prebuf_.empty())
		{
			len = (std::min)(len, prebuf_.size());
			memcpy(buf, prebuf_.data(), len);
			prebuf_ = prebuf_.substr(len);

			limit_ -= len;
			return len;
		}

		return in_.read(buf, len);
	}

private:
	std::string_view & prebuf_;
	istream & in_;
	uint64_t limit_;
};



}

void http_server(istream & in, ostream & out, std::function<response(request &&)> const & fn)
{
	char header_buf[64 * 1024];
	char * last = header_buf;
	char const * const end = header_buf + sizeof header_buf;

	for (;;)
	{
		char const * cur = header_buf;

		auto preload = [&]() {
			if (cur == last)
			{
				if (last == end)
					return false;

				size_t r = in.read(last, end - last);
				assert(r < size_t(end - last));
				last += r;

				if (r == 0)
					return false;
			}

			return true;
		};

		auto consume = [&](char ch) {
			if (!preload() || *cur != ch)
				return false;
			++cur;
			return true;
		};

		auto parse_until = [&](std::string_view & r, char sep) {
			auto first = cur;
			while (preload() && *cur != sep)
				++cur;
			if (first == last)
				return false;
			r = std::string_view(first, cur);
			++cur;
			return true;
		};

		request req;
		std::string_view version;

		auto parse_headers = [&] {
			if (
				!parse_until(req.method, ' ')
				|| !parse_until(req.path, ' ')
				|| !parse_until(version, '\r')
				|| !consume('\n'))
			{
				return false;
			}

			std::string_view line;
			while (parse_until(line, '\r') && consume('\n'))
			{
				if (line.empty())
					return true;

				size_t colon_pos = line.find(':');
				if (colon_pos == std::string_view::npos)
					return false;

				header_view hv;
				hv.name = line.substr(0, colon_pos);
				hv.value = strip(line.substr(colon_pos + 1));
				req.headers.push_back(hv);
			}

			return false;
		};

		if (!parse_headers())
		{
			if (last == header_buf)
				return;

			// Send 400 Bad Request or 413 Request Entity Too Large
		}

		std::sort(req.headers.begin(), req.headers.end());

		std::string_view prebuf(cur, last);

		bool has_body =
			req.method == std::string_view("POST")
			|| req.method == "PUT";

		if (!has_body)
		{
			req.body = std::make_shared<fixed_req_stream>(prebuf, in, 0);
		}
		else
		{
		}

		std::cerr << req.path << std::flush;

		response resp = fn(std::move(req));
		if (resp.content_length != -1)
			resp.headers.push_back({ "content-length", std::to_string(resp.content_length) });
		else
			resp.headers.push_back({ "transfer-encoding", "chunked" });

		std::string status_code = std::to_string(resp.status_code);
		if (resp.status_text.empty())
		{
			resp.status_text = "No Status Text";
			for (auto const & kv : status_texts)
			{
				if (resp.status_code == kv.first)
				{
					resp.status_text = kv.second;
					break;
				}
			}
		}

		std::cerr << " " << status_code << "\n";

		out.write_all("HTTP/1.1 ", 9);
		out.write_all(status_code.data(), status_code.size());
		out.write_all(" ", 1);
		out.write_all(resp.status_text.data(), resp.status_text.size());
		out.write_all("\r\n", 2);
		for (auto const & header : resp.headers)
		{
			out.write_all(header.name.data(), header.name.size());
			out.write_all(":", 1);
			out.write_all(header.value.data(), header.value.size());
			out.write_all("\r\n", 2);
		}
		out.write_all("\r\n", 2);

		char write_buf[64 * 1024];
		if (resp.content_length != -1)
		{
			while (resp.content_length)
			{
				size_t chunk = sizeof write_buf;
				if (chunk > resp.content_length)
					chunk = (size_t)resp.content_length;

				chunk = resp.body->read(write_buf, chunk);
				out.write_all(write_buf, chunk);

				resp.content_length -= chunk;
			}
		}
		else
		{
			for (;;)
			{
				size_t chunk = resp.body->read(write_buf, sizeof write_buf);
				if (chunk == 0)
				{
					out.write_all("0\r\n\r\n", 5);
					break;
				}

				char chunk_header[16];
				size_t chunk_header_len = 0;
				for (size_t tmp = chunk; tmp; tmp >>= 4)
				{
					static char const digits[] = "0123456789abcdef";
					chunk_header[chunk_header_len++] = digits[tmp & 0xf];
				}

				std::reverse(chunk_header, chunk_header + chunk_header_len);

				out.write_all(chunk_header, chunk_header_len);
				out.write_all("\r\n", 2);

				out.write_all(write_buf, chunk);
				out.write_all("\r\n", 2);
			}
		}

		if (!prebuf.empty())
			memmove(header_buf, prebuf.data(), prebuf.size());
		last = header_buf + prebuf.size();
	}
}

response http_abort(uint16_t status_code)
{
	return response("", {}, status_code);
}