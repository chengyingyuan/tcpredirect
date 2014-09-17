#ifndef FDEVENT_H
#define FDEVENT_H

#include "network.h"

#define FDEVENT_READ	1
#define FDEVENT_WRITE	2
#define FDEVENT_EXCEPT	4

typedef void (*FDEventReady)(void*, void*, sock_t);
typedef void (*FDEventTimeout)(void*);

struct fdevent;
typedef struct fdevent fdevent_t;

fdevent_t* fdevent_init(void *srv, struct timeval *tv, 
	FDEventTimeout cbtimeout, 
	FDEventReady cbread, 
	FDEventReady cbwrite, 
	FDEventReady cbexcept);
void fdevent_destroy(fdevent_t *ev);
void fdevent_register(fdevent_t *ev, sock_t s, void *ctx);
void fdevent_unregister(fdevent_t *ev, sock_t s);
void fdevent_set(fdevent_t *ev, sock_t s, int type);
void fdevent_clear(fdevent_t *ev, sock_t s, int type);
int fdevent_poll(fdevent_t *ev);
void fdevent_loop(fdevent_t *ev);
void fdevent_stop(fdevent_t *ev);

#endif