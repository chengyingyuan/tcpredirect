#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include "service.h"
#include "logmsg.h"
#include "rc4.h"

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

int service_conf_parse(conf_t *conf, int argc, const char* argv[])
{
	char *opt;
	char *fields[5];
	int i, c;

	conf->lefttimeout = CLIENT_TIMEOUT;
	conf->righttimeout = CLIENT_TIMEOUT;
	conf->leftbuffer = BUFFER_SIZE;
	conf->rightbuffer = BUFFER_SIZE;

		for (i = 1; i < argc; i++) {
		opt = (char *)argv[i];
		if (strcmp(opt, "-k") == 0) {
			if (i+1 >= argc) {
				logmsg("Option '-k' should have a parameter\n");
				return -1;
			}
			opt = strdup(argv[++i]);
			c = parse_conf_fields(opt, fields, 5);
			if (c != 2) {
				logmsg("Invalid format of option '-k'\n");
				return -1;
			}
			conf->leftkey = (*fields[0]) ? strdup(fields[0]):NULL;
			conf->rightkey = (*fields[1]) ? strdup(fields[1]):NULL;
			free(opt);

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

static void service_cleanup_client(service_t *s, client_t *c)
{
	if (c->lsock != NETSOCK_INVALID) {
		fdevent_unregister(s->ev, c->lsock);
	}
	if (c->rsock != NETSOCK_INVALID) {
		fdevent_unregister(s->ev, c->rsock);
	}
	logmsg("Cleaned up client(%s,%s)\n", c->ldesc, c->rdesc);
	clients_remove(s->pc, c);
	//client_destroy(c); //rb tree already freed it
}

static int service_test_complete(service_t *s, client_t *c)
{
	if (c->lstatus==CLIENT_READ_CLOSED && 
		c->rstatus==CLIENT_READ_CLOSED &&
		!(LOADSIZE(c->lbuf)) &&
		!(LOADSIZE(c->rbuf))) {
		logmsg("Client(%s,%s) completed, clean up...\n", c->ldesc, c->rdesc);
		service_cleanup_client(s, c);
		return 1;
	}
	return 0;
}

static int service_handle_read_left(service_t *s, client_t *c)
{
	switch(c->lstatus) {
	case CLIENT_READY:
		if (FREESIZE(c->lbuf)) {
			int avail = FREESIZE(c->lbuf);
			int nbytes = net_recv(c->lsock, FREEPTR(c->lbuf), avail, 0);
			if (nbytes == -1) {
				logmsg("Error read client(%s,%s) left socket\n", c->ldesc, c->rdesc);
				service_cleanup_client(s, c);

			} else {
				if (nbytes == 0) {
					c->lstatus = CLIENT_READ_CLOSED;
					fdevent_clear(s->ev, c->lsock, FDEVENT_READ);
					//logmsg("Client(%s,%s) left end of read\n", c->ldesc, c->rdesc);
					service_test_complete(s, c);

				} else {
					if (c->lrc4_in) {
						rc4_cipher(c->lrc4_in, (uint8_t *)(FREEPTR(c->lbuf)), nbytes);
					}
					if (c->rrc4_out) {
						rc4_cipher(c->rrc4_out, (uint8_t *)(FREEPTR(c->lbuf)), nbytes);
					}
					FREESTEP(c->lbuf, nbytes);
					//logmsg("Client(%s,%s) left read %d bytes\n", c->ldesc, c->rdesc, nbytes);
					if (c->rstatus==CLIENT_READY || c->rstatus==CLIENT_READ_CLOSED) {
						fdevent_set(s->ev, c->rsock, FDEVENT_WRITE);
					}
					if (!FREESIZE(c->lbuf)) {
						fdevent_clear(s->ev, c->lsock, FDEVENT_READ);
					}
				}
			}
		} else {
			logmsg("Warning Client(%s,%s) left read without space\n", 
				c->ldesc, c->rdesc);
			fdevent_clear(s->ev, c->lsock, FDEVENT_READ);		
		}
		break;
	case CLIENT_CONNECTING:
	case CLIENT_READ_CLOSED:
	case CLIENT_CLOSED:
	default:
		assert(0);
	}
	return 0;
}

static int service_handle_read_right(service_t *s, client_t *c)
{
	switch(c->rstatus) {
	case CLIENT_READY:
		if (FREESIZE(c->rbuf)) {
			int avail = FREESIZE(c->rbuf);
			int nbytes = net_recv(c->rsock, FREEPTR(c->rbuf), avail, 0);
			if (nbytes == -1) {
				logmsg("Error read client(%s,%s) right socket\n", c->ldesc, c->rdesc);
				service_cleanup_client(s, c);

			} else {
				if (nbytes == 0) {
					c->rstatus = CLIENT_READ_CLOSED;
					fdevent_clear(s->ev, c->rsock, FDEVENT_READ);
					//logmsg("Client(%s,%s) right end of read\n", c->ldesc, c->rdesc);
					service_test_complete(s, c);

				} else {
					if (c->rrc4_in) {
						rc4_cipher(c->rrc4_in, (uint8_t *)(FREEPTR(c->rbuf)), nbytes);
					}
					if (c->lrc4_out) {
						rc4_cipher(c->lrc4_out, (uint8_t *)(FREEPTR(c->rbuf)), nbytes);
					}
					FREESTEP(c->rbuf, nbytes);
					//logmsg("Client(%s,%s) right read %d bytes\n", c->ldesc, c->rdesc, nbytes);
					if (c->lstatus==CLIENT_READY || c->lstatus==CLIENT_READ_CLOSED) {
						fdevent_set(s->ev, c->lsock, FDEVENT_WRITE);
					}
					if (!FREESIZE(c->rbuf)) {
						fdevent_clear(s->ev, c->rsock, FDEVENT_READ);
					}
				}
			}
		} else {
			logmsg("Warning Client(%s,%s) right read without space\n", c->ldesc, c->rdesc);
			fdevent_clear(s->ev, c->rsock, FDEVENT_READ);		
		}
		break;
	case CLIENT_CONNECTING:
	case CLIENT_READ_CLOSED:
	case CLIENT_CLOSED:
	default:
		assert(0);
	}
	return 0;
}

static int service_handle_write_left(service_t *s, client_t *c)
{
	switch(c->lstatus) {
	case CLIENT_READY:
	case CLIENT_READ_CLOSED:
		if (LOADSIZE(c->rbuf)) {
			int avail = LOADSIZE(c->rbuf);
			int nbytes = net_send(c->lsock, LOADPTR(c->rbuf), avail, 0);
			if (nbytes == -1) {
				logmsg("Error write client(%s,%s) left socket\n", c->ldesc, c->rdesc);
				service_cleanup_client(s, c);

			} else {
				if (nbytes > 0) {
					LOADSTEP(c->rbuf, nbytes);
					//logmsg("Client(%s,%s) left write %d bytes\n", c->ldesc, c->rdesc, nbytes);
					if (!service_test_complete(s, c)) {
						if (c->rstatus == CLIENT_READY) {
							fdevent_set(s->ev, c->rsock, FDEVENT_READ);
						}
						if (!LOADSIZE(c->rbuf)) {
							fdevent_clear(s->ev, c->lsock, FDEVENT_WRITE);
						}					
					}
				}
			}		
		} else {
			logmsg("Warning Client(%s,%s) left write without load\n", c->ldesc, c->rdesc);
			fdevent_clear(s->ev, c->lsock, FDEVENT_WRITE);		
		}
		break;
	case CLIENT_CONNECTING:
	case CLIENT_CLOSED:
	default:
		assert(0);
	}
	return 0;
}

static int service_handle_write_right(service_t *s, client_t *c)
{
	switch(c->rstatus) {
	case CLIENT_CONNECTING:
		c->rstatus = CLIENT_READY;
		fdevent_set(s->ev, c->rsock, FDEVENT_READ);
		//logmsg("Client(%s,%s) right ready\n", c->ldesc, c->rdesc);
		if (!LOADSIZE(c->lbuf)) {
			fdevent_clear(s->ev, c->rsock, FDEVENT_WRITE);
		}
		break;
	case CLIENT_READY:
	case CLIENT_READ_CLOSED:
		if (LOADSIZE(c->lbuf)) {
			int avail = LOADSIZE(c->lbuf);
			int nbytes = net_send(c->rsock, LOADPTR(c->lbuf), avail, 0);
			if (nbytes == -1) {
				logmsg("Error write client(%s=>%s) right socket\n", c->ldesc, c->rdesc);
				service_cleanup_client(s, c);

			} else {
				if (nbytes > 0) {
					LOADSTEP(c->lbuf, nbytes);
					//logmsg("Client(%s,%s) right write %d bytes\n", c->ldesc, c->rdesc, nbytes);
					if (!service_test_complete(s, c)) {
						if (c->lstatus == CLIENT_READY) {
							fdevent_set(s->ev, c->lsock, FDEVENT_READ);
						}
						if (!LOADSIZE(c->lbuf)) {
							fdevent_clear(s->ev, c->rsock, FDEVENT_WRITE);
						}	
					}
				}
			}
		} else {
			logmsg("Warning Client(%s,%s) right write without load\n", 
				c->ldesc, c->rdesc);
			fdevent_clear(s->ev, c->rsock, FDEVENT_WRITE);
		}
		break;
	case CLIENT_CLOSED:
	default:
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
	p = client_init(s->conf->leftbuffer, s->conf->rightbuffer);
	p->lsock = client;
	if (net_socket_block(client, 0) != 0) {
		logmsg("ERROR Cannot set accepted socket block\n");
		client_destroy(p);
		return -1;
	}
	if (net_create_client(s->conf->rightaddr, s->conf->rightport, &p->rsock)!=0) {
		logmsg("ERROR Cannot create peer endpoint for client '%s'\n", p->ldesc);
		client_destroy(p);
		return -1;

	} else {
		p->llast = time(NULL);
		p->lstatus = CLIENT_READY;
		paddr = (struct sockaddr_in *)&addr;
		sprintf(desc, "%s:%d", inet_ntoa(paddr->sin_addr), (int)ntohs(paddr->sin_port));
		p->ldesc = strdup(desc);
		if (s->conf->leftkey) {
			p->lrc4_in = rc4_init((uint8_t*)(s->conf->leftkey), strlen(s->conf->leftkey));
			p->lrc4_out = rc4_init((uint8_t*)(s->conf->leftkey), strlen(s->conf->leftkey));
		}

		p->rlast = time(NULL);
		p->rstatus = CLIENT_CONNECTING;
		sprintf(desc, "%s:%d", s->conf->rightaddr, s->conf->rightport);
		p->rdesc = strdup(desc);
		if (s->conf->rightkey) {
			p->rrc4_in = rc4_init((uint8_t*)(s->conf->rightkey), strlen(s->conf->rightkey));
			p->rrc4_out = rc4_init((uint8_t*)(s->conf->rightkey), strlen(s->conf->rightkey));		
		}

		fdevent_register(s->ev, p->lsock, p);
		fdevent_register(s->ev, p->rsock, p);
		fdevent_set(s->ev, p->lsock, FDEVENT_READ);
		fdevent_set(s->ev, p->rsock, FDEVENT_WRITE);
	}

	clients_insert(s->pc, p);
	logmsg("Accepted client: %s(%d) => %s(%d)\n", p->ldesc, p->lsock, p->rdesc, p->rsock);
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
			if (now-c->llast >= s->conf->lefttimeout || 
				now-c->rlast >= s->conf->righttimeout) {
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
	assert((f==c->lsock || f==c->rsock));
	if (f == c->lsock) {
		service_handle_read_left(s, c);
	} else {
		service_handle_read_right(s, c);
	}
}

void service_handle_write(void *srv, void *ctx, sock_t f)
{
	service_t *s = (service_t *)srv;
	client_t *c = (client_t *)ctx;
	assert((f==c->lsock || f==c->rsock));
	if (f == c->lsock) {
		service_handle_write_left(s, c);
	} else {
		service_handle_write_right(s, c);
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
	isleft = (f == c->lsock);
	logmsg("Client(%s=>%s) %s except happened, cleanup...\n", 
		c->ldesc, c->rdesc, isleft?"left":"right");
	service_cleanup_client(s, c);
}
