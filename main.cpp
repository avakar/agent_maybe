#include "win32_socket.hpp"
#include "http_server.hpp"
#include "file.hpp"
#include "chan.hpp"
#include "tar.hpp"
#include "argparse.hpp"
#include "process.hpp"

#include <json.hpp>
using nlohmann::json;

using std::move;
using std::string_view;

struct app
{
	explicit app(std::string workspace, std::string image_name, std::string stop_cmd)
		: status_(status_t::clean), workspace_(move(workspace)), image_name_(move(image_name)),
		stop_cmd_(move(stop_cmd)), error_(0), stopping_(false)
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

		return{ r.dump(), { { "content-type", "application/json" } } };
	}

	response stop_image(request const & req)
	{
		if (stop_cmd_.empty())
			return 404;

		if (!stopping_)
		{
			stopping_ = true;
			try
			{
				error_ = run_process(stop_cmd_);
			}
			catch (...)
			{
				error_ = -1;
			}
		}
		return{ 303, { { "location", "/image" } } };
	}

	response get_tar(request const & req)
	{
		struct ctx
		{
			app & a;
			chan ch;
			tarfile_writer tf;

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

	response post_tar(request const & req)
	{
		tarfile_reader tr(*req.body);

		std::string name;
		uint64_t size;
		std::shared_ptr<istream> content;

		while (tr.next(name, size, content))
		{
			file fout;
			fout.create(join_paths(workspace_, name));
			copy(fout.out_stream(), *content);
		}

		return 200;
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
		else if (req.path == "/tar" && req.method == "POST")
		{
			return this->post_tar(req);
		}
		else
		{
			return 404;
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
	std::string stop_cmd_;
	int32_t error_;
	bool stopping_;
};

int main(int argc, char * argv[])
{
	std::string image_name;
	std::string stop_cmd;
	std::string workspace;
	int port = 8080;

	parse_argv(argc, argv, {
		{ port, "--port", 'p' },
		{ stop_cmd, "--stop-cmd" },
		{ image_name, "image-name" },
		{ workspace, "workspace" },
	});

	app a(workspace, image_name, stop_cmd);
	tcp_listen(port, [&a](istream & in, ostream & out) {
		http_server(in, out, a);
	});
}
