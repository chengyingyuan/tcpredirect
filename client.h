#ifndef CLIENT_H
#define CLIENT_H

#include "rwbuffer.h"
#include "network.h"
#include "rbtree.h"
#include "rc4.h"

#define CLIENT_UNSET	0
#define CLIENT_CONNECTING	1
#define CLIENT_READY 2
#define CLIENT_READ_CLOSED 4
#define CLIENT_CLOSED 8

// A structure to splice left and right socket
typedef struct client 
{
	struct client *self; // key for red-black-tree
	sock_t lsock;
	rwbuffer_t *lbuf;
	int lstatus; // left socket status
	int llast;	// left side of last time reading/writing
	char *ldesc; // left description addr:port
	rc4_t *lrc4_in;
	rc4_t *lrc4_out;

	sock_t rsock;
	rwbuffer_t *rbuf;
	int rstatus; // right socket status
	int rlast; // right side of last time reading/writing
	char *rdesc; // right description addr:port
	rc4_t *rrc4_in;
	rc4_t *rrc4_out;

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
