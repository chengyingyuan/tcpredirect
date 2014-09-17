#include "fdevent.h"
#include "rbtree.h"
#include "slink.h"
#include "logmsg.h"
#include <assert.h>
#include <stdlib.h>

#define IS_INVALID_FD(fd) (fd->ev == -1)
#define INVALID_FD(fd) (fd->ev = -1)

struct fdevent 
{
	void *srv;
	struct timeval *tv;
	int shutdown;
	rb_red_blk_tree *fds;
	int nfds;
	slink_t *gc;
	
	FDEventTimeout cbtimeout;
	FDEventReady cbread;
	FDEventReady cbwrite;
	FDEventReady cbexcept;
};

struct fdevent_fd
{
	sock_t s;
	int ev;
	void *ctx;
};

typedef struct fdevent_fd fdevent_fd_t;

static int rb_fd_cmp(const void *a, const void*b)
{
	sock_t s1 = *(sock_t *)a;
	sock_t s2 = *(sock_t *)b;
	if (s1 > s2) return 1;
	else if (s1 == s2) return 0;
	else return -1;
}

static void rb_fd_free(void *p)
{
}

fdevent_t* fdevent_init(void *srv, struct timeval *tv, 
	FDEventTimeout cbtimeout, 
	FDEventReady cbread, 
	FDEventReady cbwrite, 
	FDEventReady cbexcept)
{
	fdevent_t *ev = (fdevent_t *)calloc(1, sizeof(fdevent_t));
	ev->srv = srv;
	if (tv) {
		ev->tv = (struct timeval *)calloc(1, sizeof(struct timeval));
		memcpy(ev->tv, tv, sizeof(struct timeval));
	} else {
		ev->tv = NULL;
	}
	ev->cbtimeout = cbtimeout;
	ev->cbread = cbread;
	ev->cbwrite = cbwrite;
	ev->cbexcept = cbexcept;
	ev->shutdown = 0;
	ev->fds = RBTreeCreate(rb_fd_cmp, 
		rb_fd_free,
		free,
		NULL,
		NULL);
	ev->nfds = 0;
	ev->gc = slink_init();
	return ev;
}

void fdevent_destroy(fdevent_t *ev)
{
	if (ev->fds)
		RBTreeDestroy(ev->fds);
	if (ev->tv)
		free(ev->tv);
	free(ev);
}

void fdevent_register(fdevent_t *ev, sock_t s, void *ctx)
{
	rb_red_blk_node* node;
	fdevent_fd_t* fd;

	node = RBExactQuery(ev->fds, &s);
	assert(node == NULL);

	fd = (fdevent_fd_t *)calloc(1,sizeof(fdevent_fd_t));
	fd->ctx = ctx;
	fd->ev = 0;
	fd->s = s;
	node = RBTreeInsert(ev->fds, (void *)&fd->s, (void *)fd);
	ev->nfds++;
}

void fdevent_unregister(fdevent_t *ev, sock_t s)
{
	rb_red_blk_node* node;
	fdevent_fd_t *fd;

	node = RBExactQuery(ev->fds, &s);
	assert(node);
	fd = (fdevent_fd_t *)node->info;
	INVALID_FD(fd);
	//RBDelete(ev->fds, node); // Delay delete until safe
	slink_push(ev->gc, node);
	ev->nfds--;
}

void fdevent_set(fdevent_t *ev, sock_t s, int type)
{
	rb_red_blk_node* node;
	fdevent_fd_t *fd;

	node = RBExactQuery(ev->fds, &s);
	assert(node != NULL);
	fd = (fdevent_fd_t *)node->info;
	assert(!IS_INVALID_FD(fd));
	fd->ev |= type;
	//logmsg("Fd %d set to %d(new %d)\n", s, fd->ev, type);
}

void fdevent_clear(fdevent_t *ev, sock_t s, int type)
{
	rb_red_blk_node* node;
	fdevent_fd_t *fd;

	node = RBExactQuery(ev->fds, &s);
	assert(node != NULL);
	fd = (fdevent_fd_t *)node->info;
	fd->ev = fd->ev & (~type);
	//logmsg("Fd %d clear to %d(sub %d)\n", s, fd->ev, type);
}

static void fdevent_run_gc(fdevent_t *ev)
{
	while (slink_length(ev->gc) > 0) {
		rb_red_blk_node *node = (rb_red_blk_node *)slink_pop(ev->gc);
		RBDelete(ev->fds, node);
	}
}

int fdevent_poll(fdevent_t *ev)
{
	fd_set rdset, wrset, exset;
	int nready, nfds;
	struct timeval tv, *ptv;
	rb_red_blk_iter *it;
	rb_red_blk_node *node;

	fdevent_run_gc(ev); // Remove unregistered nodes

	it = RBEnumerate(ev->fds, NULL, NULL);
	node = RBEnumerateNext(it);
	FD_ZERO(&rdset);
	FD_ZERO(&wrset);
	FD_ZERO(&exset);
	nfds = 0;
	while (node != ev->fds->nil) {
		fdevent_fd_t* fd = (fdevent_fd_t *)node->info;
		if (IS_INVALID_FD(fd)) {
			abort(); // Impossible case unless buggy code
			node = RBEnumerateNext(it);
			continue;
		}
		if (fd->ev & FDEVENT_READ)
			FD_SET(fd->s, &rdset);
		if (fd->ev & FDEVENT_WRITE)
			FD_SET(fd->s, &wrset);
		if (fd->ev & FDEVENT_EXCEPT)
			FD_SET(fd->s, &exset);
		//logmsg("Polling fd %d event %d\n", fd->s, fd->ev);
#ifndef _WIN32
		if (fd->ev && fd->s > nfds) {
			nfds = fd->s;
		}
#endif
		node = RBEnumerateNext(it);
	}
	free(it);

	if (ev->tv) {
		ptv = &tv;
		ptv->tv_sec = ev->tv->tv_sec;
		ptv->tv_usec = ev->tv->tv_usec;
	} else {
		ptv = NULL;
	}
	nready = select(nfds+1, &rdset, &wrset, &exset, ptv);
	if (!nready) {
		if (ev->cbtimeout) {
			ev->cbtimeout(ev->srv);
		
		}
	} else {
		it = RBEnumerate(ev->fds, NULL, NULL);
		node = RBEnumerateNext(it);
		while (node != ev->fds->nil) {
			int type;
			fdevent_fd_t *fd;
			
			fd = (fdevent_fd_t *)node->info;
			type = fd->ev;
			if (IS_INVALID_FD(fd)) {
				node = RBEnumerateNext(it);
				continue;
			}
			if (!IS_INVALID_FD(fd) && (type & FDEVENT_READ) && FD_ISSET(fd->s, &rdset)) {
				ev->cbread(ev->srv, fd->ctx, fd->s);
			}
			if (!IS_INVALID_FD(fd) && (type & FDEVENT_WRITE) && FD_ISSET(fd->s, &wrset)) {
				ev->cbwrite(ev->srv, fd->ctx, fd->s);
			}
			if (!IS_INVALID_FD(fd) && (type & FDEVENT_EXCEPT) && FD_ISSET(fd->s, &exset)) {
				ev->cbexcept(ev->srv, fd->ctx, fd->s);
			}
			node = RBEnumerateNext(it);
		}
		free(it);
	}

	fdevent_run_gc(ev); // Remove unregistered nodes
	return nready;
}

void fdevent_loop(fdevent_t *ev)
{
	while (!ev->shutdown) {
		fdevent_poll(ev);
	}
}

void fdevent_stop(fdevent_t *ev)
{
	ev->shutdown = 1;
}
