#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include "service.h"
#include "logmsg.h"
#include "rc4.h"

#define ENV_KEY_NAME	"TCPREDIRECT_KEY"


conf_t *service_conf_init()
{
	conf_t *cf = (conf_t *)calloc(1, sizeof(conf_t));
	assert(cf);
	return cf;
}

static int parse_conf_fields(char *src, char *fields[], int max)
{
	int i = 0;
	char *p = src;
	while (i < max) {
		fields[i] = p;
		while (*p != ':' && *p) p++;
		if (*p == ':') {
			*p = '\0';
			p++;
			i++;
			if (!*p) { // *p=='xxxx:\0'
				fields[i] = p;
				i++;
				break;
			}

		} else { // *p=='xxxx\0'
			i++;
			break;
		}
	}
	return i;
}

static int parse_field_key(conf_t *conf, const char* opt)
{
	char* fields[2];
	int c;

	if (!opt) return 0;

	char* nopt = strdup(opt);
	c = parse_conf_fields(nopt, fields, sizeof(fields)/sizeof(fields[0]));
	if (c == 2) {
		if (conf->leftkey) free(conf->leftkey);
		if (conf->rightkey) free(conf->rightkey);
		conf->leftkey = (*fields[0]) ? strdup(fields[0]):NULL;
		conf->rightkey = (*fields[1]) ? strdup(fields[1]):NULL;
	}
	free(nopt);
	return ((c==2) ? 1:-1);
}

int service_conf_parse(conf_t *conf, int argc, const char* argv[])
{
	char *opt;
	char *fields[5];
	int i, c;

	conf->lefttimeout = CLIENT_TIMEOUT;
	conf->righttimeout = CLIENT_TIMEOUT;
	conf->leftbuffer = BUFFER_SIZE;
	conf->rightbuffer = BUFFER_SIZE;

	if (parse_field_key(conf, getenv(ENV_KEY_NAME)) < 0) {
		logmsg("Environment " ENV_KEY_NAME " invalid\n");
	}

		for (i = 1; i < argc; i++) {
		opt = (char *)argv[i];
		if (strcmp(opt, "-k") == 0) {
			if (i+1 >= argc) {
				logmsg("Option '-k' should have a parameter\n");
				return -1;
			}
			if (parse_field_key(conf, argv[++i]) < 0) {
				logmsg("Invalid format of option '-k'\n");
				return -1;
			}

		} else if (strcmp(opt, "-b")==0) {
			if (i+1 >= argc) {
				logmsg("Option '-b' should have a parameter\n");
				return -1;
			}
			opt = strdup(argv[++i]);
			c = parse_conf_fields(opt, fields, 5);
			if (c != 2) {
				logmsg("Invalid format of option '-b'\n");
				return -1;
			}
			conf->leftbuffer = (*fields[0]) ? atoi(fields[0]):BUFFER_SIZE;
			conf->rightbuffer = (*fields[1]) ? atoi(fields[1]):BUFFER_SIZE;
			free(opt);

		} else if (strcmp(opt, "-t")==0) {
			if (i+1 >= argc) {
				logmsg("Option '-t' should have a parameter\n");
				return -1;
			}
			opt = strdup(argv[++i]);
			c = parse_conf_fields(opt, fields, 5);
			if (c != 2) {
				logmsg("Invalid format of option '-t'\n");
				return -1;
			}
			conf->lefttimeout = (*fields[0]) ? atoi(fields[0]):CLIENT_TIMEOUT;
			conf->righttimeout = (*fields[1]) ? atoi(fields[1]):CLIENT_TIMEOUT;
			free(opt);

		} else {
			opt = strdup(opt);
			c = parse_conf_fields(opt, fields, 5);
			if (c != 4) {
				logmsg("Invalid format of address parameters\n");
				return -1;
			}
			conf->leftaddr = (*fields[0]) ? strdup(fields[0]):NULL;
			conf->leftport = (*fields[1]) ? atoi(fields[1]):0;
			conf->rightaddr = (*fields[2]) ? strdup(fields[2]):NULL;
			conf->rightport = (*fields[3]) ? atoi(fields[3]):0;
			free(opt);
			if (!conf->leftport || !conf->rightport) {
				logmsg("Left and right port should be specified\n");
				return -1;
			}
		}
	}
	if (!conf->leftport || !conf->rightport || !conf->rightaddr) {
		logmsg("Left port, right port and address should be provided\n");
		return -1;
	}
	if (conf->leftkey) {
		logmsg("Left key is set\n");
	}
	if (conf->rightkey) {
		logmsg("Right key is set\n");
	}
	return 0;
}

void service_conf_destroy(conf_t *c)
{
	if (c->leftkey) free(c->leftkey);
	if (c->rightkey) free(c->rightkey);
	if (c->leftaddr) free(c->leftaddr);
	if (c->rightaddr) free(c->rightaddr);
	free(c);
}

service_t *service_init()
{
	struct timeval tv;
	service_t *s = (service_t *)calloc(1, sizeof(service_t));
	assert(s);
	s->fdsrv = NETSOCK_INVALID;
	s->conf = service_conf_init();
	s->pc = clients_init();
	tv.tv_sec = FDEVENT_TIMEOUT;
	tv.tv_usec = 0;
	s->ev = fdevent_init(s, &tv, 
		service_handle_timeout,
		service_handle_read, 
		service_handle_write, 
		service_handle_except);
	return s;
}

void service_destroy(service_t *s)
{
	if (s->fdsrv != NETSOCK_INVALID) net_close(s->fdsrv);
	if (s->conf) service_conf_destroy(s->conf);
	if (s->pc) clients_destroy(s->pc);
	if (s->ev) fdevent_destroy(s->ev);
	free(s);
}

static void service_close_socket(service_t *s, sock_t *sock)
{
	if (*sock != NETSOCK_INVALID) {
		fdevent_unregister(s->ev, *sock);
		net_close(*sock);
		*sock = NETSOCK_INVALID;
	} else {
		//logmsg("WARN: Socket already closed\n");
	}
}
static void service_cleanup_client(service_t *s, client_t *c)
{
	service_close_socket(s, &c->left->sock);
	service_close_socket(s, &c->right->sock);
	logmsg("Cleaned up client(%s,%s)\n", 
		c->left->desc, c->right->desc);
	clients_remove(s->pc, c); // Destroy completely
}
static void service_fixup_status(service_t *s, client_t *c, partner_t* curr)
{
	partner_t *other = (c->left==curr) ? c->right:c->left;

	if (curr->status == CLIENT_READ_CLOSED) {
		if (!(other->status & (CLIENT_WRITE_CLOSED|CLIENT_CLOSED))) {
			if (!LOADSIZE(curr->buf)) { // No more bytes to write
				if (other->status==CLIENT_READ_CLOSED) {
					service_close_socket(s, &other->sock);
					other->status = CLIENT_CLOSED;
				} else {
					net_close_half(other->sock, 0); // close write
					fdevent_clear(s->ev, other->sock, FDEVENT_WRITE);
					other->status = CLIENT_WRITE_CLOSED;
				}
			}
		}
	}
	else if (curr->status == CLIENT_WRITE_CLOSED) {
		if (!(other->status & (CLIENT_READ_CLOSED|CLIENT_CLOSED))) {
			logmsg("ERROR Writing illogical\n");
			abort();
		}
	}
	else if (curr->status == CLIENT_CLOSED) {
		if (!(other->status & CLIENT_CLOSED)) {
			if (!LOADSIZE(curr->buf)) { // No more bytes to write
				service_close_socket(s, &other->sock);
				other->status = CLIENT_CLOSED;
			} else {
				logmsg("DEBUG Remains to send before close\n");
			}
		}
	}
}

static int service_test_complete(service_t *s, client_t *c)
{
	partner_t *left, *right;
	left = c->left;
	right = c->right;

	service_fixup_status(s, c, left);
	service_fixup_status(s, c, right);

	/*logmsg("Client(%s => %d,%s => %d)\n", left->desc, left->status,
		right->desc, right->status);*/
	if (left->status==CLIENT_CLOSED && right->status==CLIENT_CLOSED) {
		logmsg("Client(%s,%s) completed, clean up...\n", 
			left->desc, right->desc);
		service_cleanup_client(s, c);
		return 1;
	}
	return 0;
}

static int service_handle_client_read(service_t *s, client_t *c, partner_t* curr)
{
	partner_t *other = (c->left == curr) ? c->right:c->left;

	switch(curr->status) {
	case CLIENT_READY:
	case CLIENT_WRITE_CLOSED:
		if (FREESIZE(curr->buf)) {
			int avail = FREESIZE(curr->buf);
			int nbytes = net_recv(curr->sock, FREEPTR(curr->buf), avail, 0);
			if (nbytes == -1) {
				/* Close both sides right now */
				logmsg("Error read client(%s,%s) left socket\n", 
					curr->desc, curr->desc);
				service_cleanup_client(s, c);

			} else {
				curr->last = time(NULL);
				if (nbytes == 0) { // no bytes read
					curr->status = (curr->status==CLIENT_READY) ? 
						CLIENT_READ_CLOSED:CLIENT_CLOSED;
					fdevent_clear(s->ev, curr->sock, FDEVENT_READ);
					//logmsg("Client(%s,%s) end of read\n", curr->desc, other->desc);
					service_test_complete(s, c);

				} else {
					if (curr->rc4_in) {
						rc4_cipher(curr->rc4_in, (uint8_t *)(FREEPTR(curr->buf)), nbytes);
					}
					if (other->rc4_out) {
						rc4_cipher(other->rc4_out, (uint8_t *)(FREEPTR(curr->buf)), nbytes);
					}
					FREESTEP(curr->buf, nbytes);
					//logmsg("Client(%s,%s) read %d bytes\n", curr->desc, other->desc, nbytes);
					if (other->status & (CLIENT_READY|CLIENT_READ_CLOSED)) {
						fdevent_set(s->ev, other->sock, FDEVENT_WRITE);
					}
					if (!FREESIZE(curr->buf)) {
						fdevent_clear(s->ev, curr->sock, FDEVENT_READ);
					}
				}
			}
		} else {
			logmsg("Warning Client(%s,%s) read without space\n", 
				curr->desc, other->desc);
			fdevent_clear(s->ev, curr->sock, FDEVENT_READ);		
		}
		break;
	case CLIENT_CONNECTING:
	case CLIENT_READ_CLOSED:
	case CLIENT_CLOSED:
	default:
		logmsg("ERROR Client(%s,%s) read with status %d\n", 
			curr->desc, other->desc, curr->status);
		assert(0);
	}
	return 0;
}

static int service_handle_client_write(service_t *s, client_t *c, partner_t *curr)
{
	partner_t *other = (c->left == curr) ? c->right:c->left;

	switch(curr->status) {
	case CLIENT_READY:
	case CLIENT_READ_CLOSED:
		if (LOADSIZE(other->buf)) {
			int avail = LOADSIZE(other->buf);
			int nbytes = net_send(curr->sock, LOADPTR(other->buf), avail, 0);
			if (nbytes == -1) {
				logmsg("Error write client(%s,%s) left socket\n", 
					curr->desc, other->desc);
				service_cleanup_client(s, c);

			} else {
				if (nbytes > 0) { // wrote some bytes
					curr->last = time(NULL);
					LOADSTEP(other->buf, nbytes);
					//logmsg("Client(%s,%s) write %d bytes\n", curr->desc, other->desc, nbytes);
					if (!service_test_complete(s, c)) {
						if (other->status & (CLIENT_READY|CLIENT_WRITE_CLOSED)) {
							fdevent_set(s->ev, other->sock, FDEVENT_READ);
						}
						if (!LOADSIZE(other->buf)) {
							fdevent_clear(s->ev, curr->sock, FDEVENT_WRITE);
						}
					}
				}
			}
		} else { // no bytes to write
			logmsg("Warning Client(%s,%s) write without load\n", 
				curr->desc, other->desc);
			fdevent_clear(s->ev, curr->sock, FDEVENT_WRITE);		
		}
		break;
	case CLIENT_CONNECTING:
                curr->status = CLIENT_READY;
		curr->last = time(NULL);
                fdevent_set(s->ev, curr->sock, FDEVENT_READ);
                //logmsg("Client(%s,%s) ready\n", curr->desc, other->desc);
                if (!LOADSIZE(other->buf)) {
                        fdevent_clear(s->ev, curr->sock, FDEVENT_WRITE);
                }
                break;
	case CLIENT_WRITE_CLOSED:
	case CLIENT_CLOSED:
	default:
		logmsg("ERROR Client(%s,%s) write with status %d\n", 
			curr->desc, other->desc, curr->status);
		assert(0);
	}
	return 0;
}

static int service_accept_client(service_t *s)
{
	struct sockaddr addr;
	struct sockaddr_in *paddr;
	sock_t client;
	client_t *p;
	char desc[256];
	
	if (net_accept(s->fdsrv, &addr, &client)==-1) {
		logmsg("ERROR Accept client socket\n");
		return -1;
	}
	if (client == NETSOCK_INVALID) {
		return 0; // Try again
	}
	p = client_init(s->conf->leftbuffer, s->conf->rightbuffer);
	p->left->sock = client;
	if (net_socket_block(client, 0) != 0) {
		logmsg("ERROR Cannot set accepted socket block\n");
		client_destroy(p);
		return -1;
	}
	if (net_create_client(s->conf->rightaddr, s->conf->rightport, &p->right->sock)!=0) {
		logmsg("ERROR Cannot create right side for client '%s'\n", p->left->desc);
		client_destroy(p);
		return -1;

	} else {
		p->left->last = time(NULL);
		p->left->status = CLIENT_READY;
		paddr = (struct sockaddr_in *)&addr;
		sprintf(desc, "%s:%d", inet_ntoa(paddr->sin_addr), (int)ntohs(paddr->sin_port));
		p->left->desc = strdup(desc);
		if (s->conf->leftkey) {
			p->left->rc4_in = rc4_init((uint8_t*)(s->conf->leftkey), strlen(s->conf->leftkey));
			p->left->rc4_out = rc4_init((uint8_t*)(s->conf->leftkey), strlen(s->conf->leftkey));
		}

		p->right->last = time(NULL);
		p->right->status = CLIENT_CONNECTING;
		sprintf(desc, "%s:%d", s->conf->rightaddr, s->conf->rightport);
		p->right->desc = strdup(desc);
		if (s->conf->rightkey) {
			p->right->rc4_in = rc4_init((uint8_t*)(s->conf->rightkey), strlen(s->conf->rightkey));
			p->right->rc4_out = rc4_init((uint8_t*)(s->conf->rightkey), strlen(s->conf->rightkey));		
		}

		fdevent_register(s->ev, p->left->sock, p);
		fdevent_register(s->ev, p->right->sock, p);
		fdevent_set(s->ev, p->left->sock, FDEVENT_READ);
		fdevent_set(s->ev, p->right->sock, FDEVENT_WRITE);
	}

	clients_insert(s->pc, p);
	logmsg("Accepted client: %s(%d) => %s(%d)\n", 
		p->left->desc, p->left->sock, 
		p->right->desc, p->right->sock);
	return 0;
}

void service_handle_timeout(void *srv)
{
#define NODESSIZE	64
	rb_red_blk_node *nodes[NODESSIZE];
	int i, nnodes;
	service_t *s;
	rb_red_blk_iter *it;
	rb_red_blk_node *node;
	client_t *c;
	time_t now;

	s = (service_t *)srv;
	now = time(NULL);
	while (1) {
		it = RBEnumerate(s->pc->c, NULL, NULL);
		node = RBEnumerateNext(it);
		nnodes = 0;
		while (node != s->pc->c->nil) {
			c = (client_t *)node->info;
			if ((now - c->left->last) >= s->conf->lefttimeout ||
				(now - c->right->last) >= s->conf->righttimeout) {
				nodes[nnodes++] = node;
				if (nnodes >= NODESSIZE)
					break;
			}
			node = RBEnumerateNext(it);
		}
		//logmsg("Found %d timeout clients to cleanup\n", nnodes);
		for (i = 0; i < nnodes; i++) {
			node = nodes[i];
			c = (client_t *)node->info;
			service_cleanup_client(s, c);
		}
		if (nnodes < NODESSIZE) {
			break;
		}
	}
#undef NODESSIZE
}

void service_handle_read(void *srv, void *ctx, sock_t f)
{
	service_t *s = (service_t *)srv;
	client_t *c = (client_t *)ctx;
	if (s->fdsrv == f) {
		if (service_accept_client(s) != 0) {
			logmsg("Error accept client socket, shutting down...\n");
			fdevent_stop(s->ev);
		}
		return;
	}
	assert((f==c->left->sock || f==c->right->sock));
	if (f == c->left->sock) {
		service_handle_client_read(s, c, c->left);
	} else {
		service_handle_client_read(s, c, c->right);
	}
}

void service_handle_write(void *srv, void *ctx, sock_t f)
{
	service_t *s = (service_t *)srv;
	client_t *c = (client_t *)ctx;
	assert((f==c->left->sock || f==c->right->sock));
	if (f == c->left->sock) {
		service_handle_client_write(s, c, c->left);
	} else {
		service_handle_client_write(s, c, c->right);
	}
}

void service_handle_except(void *srv, void *ctx, sock_t f)
{
	service_t *s = (service_t *)srv;
	client_t *c = (client_t *)ctx;
	int isleft;

	if (f == s->fdsrv) {
		logmsg("Server socket except, shutting down...\n");
		fdevent_stop(s->ev);
		return;
	}
	isleft = (f == c->left->sock);
	logmsg("Client(%s=>%s)%s except happened, cleanup...\n", 
		c->left->desc, c->right->desc, isleft?"left":"right");
	service_cleanup_client(s, c);
}
