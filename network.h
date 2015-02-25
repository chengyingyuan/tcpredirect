#ifndef NETWORK_H
#define NETWORK_H

//See http://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
#ifdef _WIN32
//See http://www.codeproject.com/Articles/13071/Programming-Windows-TCP-Sockets-in-C-for-the-Begin
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable: 4996)
//#include <winsock.h>
#include <winsock2.h>
typedef SOCKET sock_t;
#define NETSOCK_INVALID INVALID_SOCKET

#elif __linux
typedef int sock_t;
#define NETSOCK_INVALID -1

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#else
#error("Platform not supported")
#endif

int net_recv(sock_t s, void *buf, size_t len, int flags);
int net_send(sock_t s, void *buf, size_t len, int flags);
int net_accept(sock_t s, struct sockaddr *addr, sock_t *c);
int net_close(sock_t s);
int net_close_half(sock_t s, int read);

int net_create_server(const char *host, int port, sock_t *s);
int net_create_client(const char *host, int port, sock_t *s);
int net_socket_block(sock_t s, int block);

#endif
