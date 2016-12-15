#include "json.hpp"
using json = nlohmann::json;

#include <string>
#include <vector>

struct buffer
{
};

template <typename T>
struct ichan
{
	virtual T recv() = 0;
};

using istream = ichan<buffer>;

struct header
{
	std::string key;
	std::string value;
};

struct response
{
	uint16_t status_code;
	std::string status_text;
	std::vector<header> headers;
	std::shared_ptr<istream> body;

	response(std::shared_ptr<istream> body, std::initializer_list<header> headers, uint16_t status_code = 200)
		: status_code(status_code), headers(headers), body(body)
	{
	}

	response(std::string body, std::initializer_list<header> headers, uint16_t status_code = 200)
		: status_code(status_code), headers(headers)
	{
	}
};

struct request
{
	std::string method;
	std::string path;
	std::vector<header> headers;
	std::shared_ptr<istream> body;
};

response get_stats()
{
	std::string id = "test";

	json obj = {
		{ "agent_id", id },
		};

	return { obj.dump(), { { "content-type", "application/json"} } };
}

#include <winsock2.h>
#include <windows.h>

void client_handler(SOCKET sock)
{
	char buf[4096];
	int r = recv(sock, buf, sizeof buf, 0);

}

#include <thread>

int main()
{
	WSADATA wsd;
	WSAStartup(MAKEWORD(2, 2), &wsd);

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(8080);
	bind(listen_sock, reinterpret_cast<sockaddr const *>(&bind_addr), sizeof bind_addr);

	listen(listen_sock, SOMAXCONN);

	for (;;)
	{
		SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
		std::thread thr([client_sock] {
			client_handler(client_sock);
		});

		thr.detach();
	}
}
