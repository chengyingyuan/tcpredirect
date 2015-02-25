#include <stdlib.h>
#include <assert.h>
#include "compiler.h"
#include "client.h"

client_t* client_init(int lbuflen, int rbuflen)
{
		client_t *p = (client_t *)calloc(1, sizeof(client_t));
		assert(p);
		p->self = p;
		p->left = (partner_t *)calloc(1, sizeof(partner_t));
		p->right = (partner_t *)calloc(1,sizeof(partner_t));
		assert(p->left);
		assert(p->right);
		p->left->buf = rwbuffer_init(lbuflen);
		p->right->buf = rwbuffer_init(rbuflen);
		p->left->sock = NETSOCK_INVALID;
		p->right->sock = NETSOCK_INVALID;
		p->left->status = CLIENT_UNSET;
		p->right->status = CLIENT_UNSET;
		p->left->last = 0;
		p->right->last = 0;
		return p;
}

void client_destroy(void *p)
{
	client_t *c = (client_t *)p;
	if (c->left->buf) rwbuffer_destroy(c->left->buf);
	if (c->left->desc) free(c->left->desc);
	if (c->right->buf) rwbuffer_destroy(c->right->buf);
	if (c->right->desc) free(c->right->desc);
	if (c->left->rc4_in) rc4_destroy(c->left->rc4_in);
	if (c->left->rc4_out) rc4_destroy(c->left->rc4_out);
	if (c->right->rc4_in) rc4_destroy(c->right->rc4_in);
	if (c->right->rc4_out) rc4_destroy(c->right->rc4_out);
	if (c->left->sock != NETSOCK_INVALID) net_close(c->left->sock);
	if (c->right->sock != NETSOCK_INVALID) net_close(c->right->sock);
	free(c->left);
	free(c->right);
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
	UNUSED(p);
}

clients_t* clients_init()
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
