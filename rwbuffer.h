#ifndef RWBUFFER_H
#define RWBUFFER_H

typedef struct struct_rwbuffer
{
	void *buf;
	int size;
	int rptr; // read pointer
	int wptr; // write pointer

} rwbuffer_t;

#define LOADSIZE(b) rwbuffer_loadsize(b)
#define LOADPTR(b) ((char*)b->buf+b->rptr)
#define LOADSTEP(b,n) rwbuffer_loadstep(b,n)

#define FREESIZE(b) rwbuffer_freesize(b)
#define FREEPTR(b) ((char*)b->buf+b->wptr)
#define FREESTEP(b,n) rwbuffer_freestep(b,n)

rwbuffer_t* rwbuffer_init(int size);
void rwbuffer_destroy(rwbuffer_t *b);
int rwbuffer_loadsize(rwbuffer_t *b);
int rwbuffer_freesize(rwbuffer_t *b);
void rwbuffer_loadstep(rwbuffer_t *b, int steps);
void rwbuffer_freestep(rwbuffer_t *b, int steps);

#endif