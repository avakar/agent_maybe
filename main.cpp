#include "win32_socket.hpp"
#include "http_server.hpp"

#include "json.hpp"
using nlohmann::json;

using std::move;
using std::string_view;

struct tarfile
{
	explicit tarfile(ostream & out)
		: out_(out)
	{
	}

	void add(string_view name, uint64_t size, istream & file)
	{
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
		return{ std::make_shared<tar_stream>(workspace_), { { "content-type", "application/x-tar" } } };
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
			return this->stop_image(req);
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
	app a("c:\\devel\\checkouts\\remote_test_agent\\ws", "win10-wbam");
	tcp_listen(8080, [&a](istream & in, ostream & out) {
		http_server(in, out, a);
	});
}
