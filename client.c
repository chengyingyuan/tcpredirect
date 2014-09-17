#include <stdlib.h>
#include <assert.h>
#include "client.h"

client_t* client_init(int lbuflen, int rbuflen)
{
		client_t *p = (client_t *)calloc(1, sizeof(client_t));
		assert(p);
		p->self = p;
		p->lbuf = rwbuffer_init(lbuflen);
		p->rbuf = rwbuffer_init(rbuflen);
		p->lsock = NETSOCK_INVALID;
		p->rsock = NETSOCK_INVALID;
		p->lstatus = CLIENT_UNSET;
		p->rstatus = CLIENT_UNSET;
		p->llast = 0;
		p->rlast = 0;
		return p;
}

void client_destroy(void *p)
{
	client_t *c = (client_t *)p;
	if (c->lbuf) rwbuffer_destroy(c->lbuf);
	if (c->ldesc) free(c->ldesc);
	if (c->rbuf) rwbuffer_destroy(c->rbuf);
	if (c->rdesc) free(c->rdesc);
	if (c->lrc4_in) rc4_destroy(c->lrc4_in);
	if (c->lrc4_out) rc4_destroy(c->lrc4_out);
	if (c->rrc4_in) rc4_destroy(c->rrc4_in);
	if (c->rrc4_out) rc4_destroy(c->rrc4_out);
	if (c->lsock != NETSOCK_INVALID) net_close(c->lsock);
	if (c->rsock != NETSOCK_INVALID) net_close(c->rsock);
	free(c);
}

static int rb_client_cmp(const void *a, const void *b)
{
	client_t *c1 = *(client_t **)a;
	client_t *c2 = *(client_t **)b;
	if (c1 == c2) return 0;
	else if (c1 > c2) return 1;
	else return -1;
}

static void rb_client_free_key(void *p)
{
}

clients_t* clients_init(int size)
{
	clients_t *p = (clients_t *)calloc(1, sizeof(clients_t));
	assert(p);
	p->c = RBTreeCreate(rb_client_cmp, rb_client_free_key, client_destroy, NULL, NULL);
	p->count = 0;
	return p;
}

void clients_destroy(clients_t *p)
{
	RBTreeDestroy(p->c);
	free(p);
}

void clients_insert(clients_t *p, client_t *c)
{
	rb_red_blk_node *node = RBExactQuery(p->c, c);
	assert(node == NULL);
	RBTreeInsert(p->c, &c->self, c);
	p->count++;
}

void clients_remove(clients_t *p, client_t *c)
{
	rb_red_blk_node *node = RBExactQuery(p->c, c);
	assert(node != NULL);
	RBDelete(p->c, node);
	p->count--;
}
