#include <stdlib.h>
#include <assert.h>
#include "rwbuffer.h"

rwbuffer_t* rwbuffer_init(int size)
{
	rwbuffer_t *b = (rwbuffer_t *)calloc(1, sizeof(rwbuffer_t));
	assert(b);
	b->buf = malloc(size);
	assert(b->buf);
	b->rptr = b->wptr = 0;
	b->size = size;
	return b;
}

void rwbuffer_destroy(rwbuffer_t *b)
{
	if (b->buf) {
		free(b->buf);
	}
	free(b);
}


int rwbuffer_loadsize(rwbuffer_t *b)
{
	if (b->rptr == b->wptr) { // Empty
		return 0;
	}
	if (b->rptr < b->wptr) { // r....w
		return b->wptr - b->rptr;
	} else { // w....r....
		return b->size - b->rptr;
	}
}

int rwbuffer_freesize(rwbuffer_t *b)
{
	if (((b->wptr+1) % b->size) == b->rptr) { // Full: ....wr....
		return 0;
	}
	if (b->wptr < b->rptr) { // w....r
		return b->rptr - b->wptr - 1;
	} else { // r....w....
		if (b->rptr != 0) {
			return b->size - b->wptr;
		} else {
			return b->size - b->wptr - 1;
		}
	}
}

void rwbuffer_loadstep(rwbuffer_t *b, int steps)
{
	b->rptr += steps;
	b->rptr %= b->size;
}

void rwbuffer_freestep(rwbuffer_t *b, int steps)
{
	b->wptr += steps;
	b->wptr %= b->size;
}
