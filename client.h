#ifndef CLIENT_H
#define CLIENT_H

#include <time.h>
#include "rwbuffer.h"
#include "network.h"
#include "rbtree.h"
#include "rc4.h"

#define CLIENT_UNSET	0
#define CLIENT_CONNECTING	1
#define CLIENT_READY 2
#define CLIENT_READ_CLOSED 4
#define CLIENT_WRITE_CLOSED 8
#define CLIENT_CLOSED 16

// A structure to splice left and right socket
typedef struct partner 
{
	sock_t sock;
	rwbuffer_t *buf;
	int status; // left socket status
	time_t last;	// left side of last time reading/writing
	char *desc; // left description addr:port
	rc4_t *rc4_in;
	rc4_t *rc4_out;

} partner_t;

typedef struct client 
{
	struct client *self; // key for red-black-tree
	partner_t* left;
	partner_t* right;

} client_t;

typedef struct clients 
{
	rb_red_blk_tree *c; // all clients
	int count;

} clients_t;


client_t* client_init(int lbuf_size, int rbuf_size);
void client_destroy(void *p);
clients_t* clients_init();
void clients_destroy(clients_t *p);
void clients_insert(clients_t *p, client_t *c);
void clients_remove(clients_t *p, client_t *c);

#endif
