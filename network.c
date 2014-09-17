#include "network.h"
#include "logmsg.h"

#ifdef _WIN32

int net_accept(sock_t s, struct sockaddr *addr, sock_t *c)
{
	int addrlen;
	sock_t rc;

	addrlen = sizeof(struct sockaddr);
	memset((void *)addr, 0, addrlen);

	rc = accept(s, addr, &addrlen);
	if (rc == INVALID_SOCKET) {
		logmsg("ERROR Accept client socket(error %d)\n", WSAGetLastError());
		return -1;
	}
	*c = rc;
	return 0;
}

int net_close(sock_t s)
{
	return closesocket(s);
}

int net_recv(sock_t s, void *buf, size_t len, int flags)
{
	int r = recv(s, (char *)buf, len, flags);
	if (r == SOCKET_ERROR) {
		logmsg("ERROR read socket %d, code %d\n", s, WSAGetLastError());
		return -1;
	}
	return r;
}

int net_send(sock_t s, void *buf, size_t len, int flags)
{
	int r = send(s, (char *)buf, len, flags);
	if (r == SOCKET_ERROR) {
		logmsg("ERROR write socket %d, code %d\n", s, WSAGetLastError());
		return -1;
	}
	return r;
}

int net_socket_block(sock_t s, int block)
{
	u_long iMode = block ? 0:1;
	if (ioctlsocket(s,FIONBIO,&iMode) == SOCKET_ERROR) {
		logmsg("Error set socket(%d) block(%d), code %d\n", s, block, WSAGetLastError);
		return -1;
	}
	return 0;
}

int net_create_server(const char *host, int port, sock_t *s)
{
	sock_t rs;
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!host) {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		addr.sin_addr.s_addr = inet_addr(host);
	}

	rs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rs == INVALID_SOCKET) {
		logmsg("ERROR Create server socket(code %d)\n", WSAGetLastError());
		return -1;
	}
	if (net_socket_block(rs, 0) != 0) {
		logmsg("ERROR Set server socket block\n");
		closesocket(rs);
		return -1;
	}
	if (bind(rs, (LPSOCKADDR)&addr, sizeof(addr))==SOCKET_ERROR) {
		logmsg("ERROR Bind server socket(code %d)\n", WSAGetLastError());
		closesocket(rs);
		return -1;
	}
	if (listen(rs, SOMAXCONN)==SOCKET_ERROR) {
		logmsg("ERROR Listen on server socket(code %d)\n", WSAGetLastError());
		closesocket(rs);
		return -1;
	}
	*s = rs;
	return 0;
}

int net_create_client(const char *host, int port, sock_t *s)
{
	sock_t rs;
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!host) {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		addr.sin_addr.s_addr = inet_addr(host);
	}

	rs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rs == INVALID_SOCKET) {
		logmsg("ERROR Create client socket(code %d)\n", WSAGetLastError());
		return -1;
	}
	if (net_socket_block(rs, 0) != 0) {
		logmsg("Error Set client socket block\n");
		closesocket(rs);
		return -1;
	}
	if (connect(rs, (SOCKADDR *)&addr, sizeof(addr))==SOCKET_ERROR) {
		int ec = WSAGetLastError();
		if (ec != WSAEWOULDBLOCK) {
			logmsg("ERROR Connect to remote host(code %d)\n", WSAGetLastError());
			closesocket(rs);
			return -1;
		}
	}
	*s = rs;
	return 0;
}

#elif __linux

int net_accept(sock_t s, struct sockaddr *addr, sock_t *c)
{
	socklen_t addrlen;
	int rc;

	addrlen = sizeof(struct sockaddr);
	memset((void*)addr, 0, addrlen);
	rc = accept(s, addr, &addrlen);
	if (rc == -1) {
		logmsg("ERROR Accept client socket - %d:%s\n", errno, strerror(errno));
		return -1;
	}
	*c = rc;
	return 0;
}

int net_close(sock_t s)
{
	return close(s);
}

int net_recv(sock_t s, void *buf, size_t len, int flags)
{
	int r = recv(s, (char *)buf, len, flags);
	if (r == -1) {
		logmsg("ERROR read socket %d - %d:%s\n", s, errno, strerror(errno));
		return -1;
	}
	return r;
}

int net_send(sock_t s, void *buf, size_t len, int flags)
{
	int r = send(s, (char *)buf, len, flags);
	if (r == -1) {
		logmsg("ERROR write socket %d - %d:%s\n", s, errno, strerror(errno));
		return -1;
	}
	return r;
}

int net_socket_block(sock_t s, int block)
{
	int flags = fcntl(s, F_GETFL, 0);
	if (!block) {
		flags |= O_NONBLOCK;
	} else {
		flags &= (~O_NONBLOCK);
	}
	if (fcntl(s, F_SETFL, flags)==-1) {
		int en = errno;
		logmsg("ERROR set socket(%d) block(%d), code %d\n", s, block, en);
		return -1;
	}
	return 0;
}

int net_create_server(const char *host, int port, sock_t *s)
{
	sock_t rs;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!host) {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		addr.sin_addr.s_addr = inet_addr(host);
	}

	rs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rs == -1) {
		logmsg("ERROR Create server socket - %d:%s\n", errno, strerror(errno));
		return -1;
	}
	if (net_socket_block(rs, 0) != 0) {
		logmsg("ERROR Set server socket non-block\n");
		net_close(rs);
		return -1;
	}
	if (bind(rs, (const struct sockaddr*)&addr, sizeof(addr))==-1) {
		logmsg("ERROR Bind server socket - %d:%s\n",  errno, strerror(errno));
		net_close(rs);
		return -1;
	}
	if (listen(rs, SOMAXCONN)==-1) {
		logmsg("ERROR Listen on server socket - %d:%s\n", errno, strerror(errno));
		net_close(rs);
		return -1;
	}
	*s = rs;
	return 0;
}

int net_create_client(const char *host, int port, sock_t *s)
{
	sock_t rs;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!host) {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		addr.sin_addr.s_addr = inet_addr(host);
	}

	rs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rs == -1) {
		logmsg("ERROR Create client socket - %d:%s\n", errno, strerror(errno));
		return -1;
	}
	if (net_socket_block(rs, 0) != 0) {
		logmsg("Error Set client socket non-block\n");
		net_close(rs);
		return -1;
	}
	if (connect(rs, (const struct sockaddr *)&addr, sizeof(addr))==-1) {
		int ec = errno;
		if (ec != EINPROGRESS) {
			logmsg("ERROR Connect to remote host - %d:%s\n", errno, strerror(errno));
			net_close(rs);
			return -1;
		}
	}
	*s = rs;
	return 0;
}

#endif
