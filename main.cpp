#include "win32_socket.hpp"
#include "http_server.hpp"
#include "file.hpp"
#include "chan.hpp"
#include "tar.hpp"
#include "argparse.hpp"
#include "process.hpp"
#include "format.hpp"
#include "guid.hpp"
#include "known_paths.hpp"
#include "tls.hpp"

#include <mutex>

#include <string_utils.hpp>

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
		auto appdata = get_appdata_dir();
		state_file_ = appdata + "/remote_test_agent.json";

		{
			file fin;
			std::error_code ec;
			fin.open_ro(state_file_, ec);

			if (!ec)
			{
				if (!this->parse_state_file(fin.in_stream()))
					ec = std::make_error_code(std::errc::no_such_file_or_directory);
			}

			if (ec == std::errc::no_such_file_or_directory)
			{
				agent_uuid_ = new_uuid();
				session_count_ = 0;
			}
			else if (ec)
			{
				throw std::system_error(ec);
			}
		}

		session_count_ += 1;
		this->save_state_file();
	}

	bool parse_state_file(istream & in)
	{
		json j = json::parse(in.read_all());

		if (!j.is_object())
			return false;

		auto agent_id = j.find("agent_uuid");
		if (agent_id == j.end() || !agent_id->is_string())
			return false;

		auto session_count = j.find("session_count");
		if (session_count == j.end() || !session_count->is_number_unsigned())
			return false;

		agent_uuid_ = agent_id->get<std::string>();
		session_count_ = session_count->get<size_t>();
		return true;
	}

	void save_state_file()
	{
		json j = {
			{ "agent_uuid", agent_uuid_ },
			{ "session_count", session_count_ },
		};

		file fout;
		fout.create(state_file_);
		fout.out_stream().write_all(j.dump());
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
		auto body = make_istream([this](ostream & out) {
			//filter_writer<gzip_filter> gz(out, /*compress=*/true);
			tarfile_writer tf(out);
			enum_files(this->workspace_, [this, &tf](std::string_view fname) {
				file fin;
				fin.open_ro(join_paths(this->workspace_, fname));
				tf.add(fname, fin.size(), fin.mtime(), fin.in_stream());
			});
			tf.close();
		});

		return{ body, { { "content-type", "application/x-tar" } } };
	}

	response post_tar(request const & req)
	{
		auto go = [this](tarfile_reader & tr) {
			std::string name;
			uint64_t size;
			std::shared_ptr<istream> content;

			while (tr.next(name, size, content))
			{
				file fout;
				fout.create(join_paths(workspace_, name));
				copy(fout.out_stream(), *content);
			}
		};

		auto * ct = get_single(req.headers, "content-type");
		if (ct && *ct == "application/x-gzip")
		{
			filter_reader<gzip_filter> gz(*req.body, /*compress=*/false);
			tarfile_reader tr(gz);
			go(tr);
			return 200;
		}
		else if (ct && *ct == "application/x-tar")
		{
			tarfile_reader tr(*req.body);
			go(tr);
			return 200;
		}
		else
		{
			return 406;
		}
	}

	response get_file(request const & req, string_view name)
	{
		auto body = std::make_shared<file>();

		std::error_code err;
		body->open_ro(name, err);

		if (err == std::errc::no_such_file_or_directory)
			return{ 404 };

		if (err)
			return{ 500 };

		return{ std::shared_ptr<istream>(body, &body->in_stream()), { { "content-type", "application/octet-stream" } } };
	}

	response delete_tree(request const & req)
	{
		std::error_code ec;
		rmtree(workspace_, ec);
		if (ec)
			return{ ec.message().c_str(), { { "content-type", "text/plain" } }, 500 };
		else
			return 200;
	}

	response start_exec(request const & req)
	{
		json j = json::parse(req.body->read_all());

		auto cmd = j.find("cmd");
		if (cmd == j.end() || !cmd->is_array())
			return 400;

		auto pure = j.find("pure");
		if (pure == j.end() || !pure->is_boolean())
			return 400;

		proc_info pi;

		std::string cmdline;
		for (auto && e : *cmd)
		{
			if (!e.is_string())
				return 400;
			append_cmdline(cmdline, e.get<std::string>());
			pi.cmd.push_back(e);
		}

		pi.pure = pure->get<bool>();
		pi.proc = std::make_unique<process>();

		std::lock_guard<std::mutex> l(mutex_);
		processes_.push_back(std::move(pi));

		auto && proc = processes_.back();
		proc.proc->start(cmdline);

		if (!proc.pure)
			status_ = status_t::unpure;

		std::string new_url = format("exec/{}-{}", agent_uuid_, processes_.size() - 1);
		response resp = this->get_exec(proc, processes_.size() - 1);
		resp.status_code = 201;
		resp.headers.push_back({ "location", new_url });
		return resp;
	}

	response get_exec(request const & req, std::string_view id)
	{
		if (id.size() < 37 || !starts_with(id, agent_uuid_) || id[36] != '-')
			return 404;

		id = id.substr(37);

		size_t conv_idx;
		long lid = std::stoi(id, &conv_idx);
		if (conv_idx < id.size() || lid >= processes_.size())
			return 404;

		auto && pi = processes_[lid];
		return this->get_exec(pi, lid);
	}

	response route(request const & req)
	{
		if (starts_with(req.path, "/files/") && req.method == "GET")
		{
			return this->get_file(req, req.path.substr(7));
		}
		else if (req.path == "/exec/" && req.method == "POST")
		{
			return this->start_exec(req);
		}
		else if (starts_with(req.path, "/exec/") && req.method == "GET")
		{
			return this->get_exec(req, req.path.substr(6));
		}
		else if (req.path == "/image" && req.method == "GET")
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
		else if (req.path == "/tree" && req.method == "DELETE")
		{
			return this->delete_tree(req);
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

	struct proc_info
	{
		std::vector<std::string> cmd;
		bool pure;
		std::unique_ptr<process> proc;
	};

	response get_exec(proc_info const & pi, size_t id)
	{
		if (!pi.proc->poll())
		{
			json r = {
				{ "id", id },
				{ "command", pi.cmd },
				{ "exit_code", json() },
				{ "pure", pi.pure },
			};

			return{ r.dump(), { { "content-type", "application/json" } } };
		}
		else
		{
			json r = {
				{ "id", id },
				{ "command", pi.cmd },
				{ "exit_code", pi.proc->exit_code() },
				{ "pure", pi.pure },
			};

			return{ r.dump(), { { "content-type", "application/json" } } };
		}
	}

	std::mutex mutex_;

	std::string state_file_;
	std::string agent_uuid_;
	size_t session_count_;

	status_t status_;
	std::string workspace_;
	std::string image_name_;
	std::string stop_cmd_;
	int32_t error_;
	bool stopping_;

	std::vector<proc_info> processes_;
};

int main(int argc, char * argv[])
{
	std::string image_name;
	std::string stop_cmd;
	std::string tls_key, tls_cert;
	std::string workspace;
	int port = 8080;

	parse_argv(argc, argv, {
		{ port, "--port", 'p' },
		{ stop_cmd, "--stop-cmd" },
		{ tls_key, "--tls-key" },
		{ tls_cert, "--tls-cert" },
		{ image_name, "image-name" },
		{ workspace, "workspace" },
	});

	app a(workspace, image_name, stop_cmd);
	if (tls_key.empty() || tls_cert.empty())
	{
		tcp_listen(port, [&a](istream & in, ostream & out) {
			http_server(in, out, std::ref(a));
		});
	}
	else
	{
		tcp_listen(port, [&a, &tls_key, &tls_cert](istream & in, ostream & out) {
			std::shared_ptr<istream> in_tls;
			std::shared_ptr<ostream> out_tls;
			std::string proto = tls_server(in_tls, out_tls, in, out, tls_key, tls_cert, { "h2", "http/1.1" });

			if (proto != "h2")
				http_server(*in_tls, *out_tls, std::ref(a));
			else
				http2_server(*in_tls, *out_tls, std::ref(a));
		});
	}
}
