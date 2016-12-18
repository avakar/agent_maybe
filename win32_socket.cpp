#include "win32_socket.hpp"
#include <winsock2.h>
#include <windows.h>
#include <thread>

namespace {

struct win32_socket final
	: istream, ostream
{
	explicit win32_socket(SOCKET sock)
		: m_socket(sock)
	{
	}

	~win32_socket()
	{
		closesocket(m_socket);
	}

	size_t read(char * buf, size_t len) override
	{
		if (len > MAXINT)
			len = MAXINT;
		return ::recv(m_socket, buf, (int)len, 0);
	}

	size_t write(char const * buf, size_t len) override
	{
		if (len > MAXINT)
			len = MAXINT;
		return ::send(m_socket, buf, (int)len, 0);
	}

private:
	SOCKET m_socket;
};

}

void tcp_listen(uint16_t port, std::function<void (istream & in, ostream & out)> handler)
{
	WSADATA wsd;
	WSAStartup(MAKEWORD(2, 2), &wsd);

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(port);
	bind(listen_sock, reinterpret_cast<sockaddr const *>(&bind_addr), sizeof bind_addr);

	listen(listen_sock, SOMAXCONN);

	for (;;)
	{
		SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
		std::thread thr([client_sock, handler] {
			try
			{
				win32_socket sock(client_sock);
				handler(sock, sock);
			}
			catch (...)
			{
			}
		});

		thr.detach();
	}
}
