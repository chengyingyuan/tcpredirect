#ifndef SERVICE_H
#define SERVICE_H

#include "network.h"
#include "fdevent.h"
#include "client.h"

#define BUFFER_SIZE		8192
#define FDEVENT_TIMEOUT	5
#define CLIENT_TIMEOUT	30

typedef struct conf
{
	char *leftaddr;
	int leftport;
	char *rightaddr;
	int rightport;
	char *leftkey;
	char *rightkey;
	int lefttimeout;
	int righttimeout;
	int leftbuffer;
	int rightbuffer;

} conf_t;

typedef struct service
{
	sock_t fdsrv;
	conf_t *conf;
	clients_t *pc;
	fdevent_t *ev;

} service_t;

conf_t *service_conf_init();
void service_conf_destroy(conf_t *c);
int service_conf_parse(conf_t *cf, int argc, const char* argv[]);

service_t *service_init();
void service_destroy(service_t *s);
void service_handle_timeout(void *srv);
void service_handle_read(void *srv, void *ctx, sock_t f);
void service_handle_write(void *srv, void *ctx, sock_t f);
void service_handle_except(void *srv, void *ctx, sock_t f);

#endif