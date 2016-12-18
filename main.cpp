#include "win32_socket.hpp"
#include "http_server.hpp"
#include "file.hpp"
#include "chan.hpp"

#include "json.hpp"
using nlohmann::json;

using std::move;
using std::string_view;

static void write_oct(char * buf, size_t len, uint64_t num)
{
	buf[--len] = ' ';

	while (len)
	{
		buf[--len] = '0' + (num & 0x7);
		num >>= 3;
	}
}

static char const g_empty_two_blocks[1024] = {};

struct tarfile
{
	explicit tarfile(ostream & out)
		: out_(out)
	{
	}

	void add(string_view name, uint64_t size, uint32_t mtime, istream & file)
	{
		char buf[16*1024] = {};

		if (name.size() > 100)
			throw std::runtime_error("tar name too long");

		// name
		memcpy(buf, name.data(), name.size());

		// mode
		memcpy(buf + 100, "000666 ", 7);

		// uid, gid
		memcpy(buf + 108, "000000 ", 7);
		memcpy(buf + 116, "000000 ", 7);

		// size
		write_oct(buf + 124, 12, size);

		// mtime
		write_oct(buf + 136, 12, mtime);

		// typeflag
		buf[156] = '0';

		// magic+version
		memcpy(buf + 257, "ustar\x0000", 8);

		// chksum
		write_oct(buf + 148, 8, std::accumulate(buf, buf + 512, (size_t)(0x20 * 8)));

		out_.write_all(buf, 512);

		size_t padding = 512 - (size % 512);
		if (padding == 512)
			padding = 0;

		while (size)
		{
			size_t chunk = (std::min)(size, sizeof buf);
			size_t r = file.read(buf, chunk);
			assert(r != 0);

			size -= chunk;
			out_.write_all(buf, chunk);
		}

		if (padding)
		{
			memset(buf, 0, padding);
			out_.write_all(buf, padding);
		}
	}

	void close()
	{
		out_.write_all(g_empty_two_blocks, sizeof g_empty_two_blocks);
	}

private:
	ostream & out_;
};

struct tar_stream
	: istream
{
	explicit tar_stream(std::string workspace)
		: workspace_(workspace)
	{
	}

	size_t read(char * buf, size_t len) override
	{
		return 0;
	}

private:
	std::string workspace_;
};

struct app
{
	explicit app(std::string workspace, std::string image_name)
		: status_(status_t::clean), workspace_(workspace), image_name_(move(image_name)), stopping_(false)
	{
	}

	response get_image(request const & req)
	{
		string_view status;
		if (stopping_)
		{
			status = "stopping";
		}
		else
		{
			switch (status_)
			{
			case status_t::clean:
				status = "clean";
				break;
			case status_t::dirty:
				status = "dirty";
				break;
			case status_t::unpure:
				status = "unpure";
				break;
			}
		}

		json r = {
			{ "status", std::string(status) },
			{ "name", image_name_ },
		};

		return response(r.dump(), { { "content-type", "application/json" } });
	}

	response stop_image(request const & req)
	{
		if (!stopping_)
		{
			// TODO: exec stop
			stopping_ = true;
		}
		return{ 303, { { "location", "/image" } } };
	}

	response get_tar(request const & req)
	{
		struct ctx
		{
			app & a;
			chan ch;
			tarfile tf;

			ctx(app & a)
				: a(a), tf(ch)
			{
				ch.add_coroutine([this] {
					enum_files(this->a.workspace_, [this](std::string_view fname) {
						file fin;
						fin.open_ro(join_paths(this->a.workspace_, fname));
						tf.add(fname, fin.size(), fin.mtime(), fin.in_stream());
					});

					tf.close();
				});
			}
		};

		auto px = std::make_shared<ctx>(*this);
		return{ std::shared_ptr<istream>(px, &px->ch), { { "content-type", "application/x-tar" } } };
	}

	response route(request const & req)
	{
		if (req.path == "/image" && req.method == "GET")
		{
			return this->get_image(req);
		}
		else if (req.path == "/image/stop" && req.method == "POST")
		{
			return this->stop_image(req);
		}
		else if (req.path == "/tar" && req.method == "GET")
		{
			return this->get_tar(req);
		}
		else
		{
			return http_abort(404);
		}
	}

	response operator()(request req)
	{
		if (req.method == "HEAD")
			req.method = "GET";
		return this->route(req);
	}

private:
	enum class status_t { clean, dirty, unpure };

	status_t status_;
	std::string workspace_;
	std::string image_name_;
	bool stopping_;
};

int main()
{
	app a("c:\\devel\\checkouts\\agent_maybe\\ws", "win10-wbam");
	tcp_listen(8080, [&a](istream & in, ostream & out) {
		http_server(in, out, a);
	});
}
