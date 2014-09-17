#include <stdlib.h>
#include <string.h>
#include "array.h"

static void array_alloc(array_t *a, int hint)
{
	int base, best;

	base = a->size / a->dsize;
	if (!base) {
		base = 1;
	}
	while (base * a->dsize < hint) {
		base *= 2;
	}
	best = base;
	while (base * a->dsize > hint) {
		best = base;
		base /= 2;
	}
	base = best;
	a->size = base *a->dsize;
	a->mem = realloc(a->mem, a->size);
}

array_t *array_init(int dsize, int init_count)
{
	array_t *a = (array_t *)calloc(1, sizeof(array_t));
	a->dsize = dsize;
	if (init_count < 0) {
		init_count = ARRAY_INIT_COUNT;
	}
	if (init_count > 0) {
		array_alloc(a, init_count * dsize);
	}
	return a;
}

void array_destroy(array_t *a)
{
	if (a->mem) {
		free(a->mem);
	}
	free(a);
}

void array_put(array_t *a, void *d)
{
	int total = a->dsize * (a->count+1);
	if (a->size < total) {
		array_alloc(a, total);
	}
	memcpy((char *)a->mem+a->count*a->dsize, d, a->dsize);
	a->count++;
}

void array_set(array_t *a, int i, void *d)
{
	if (a->count <= i) {
		int total = a->dsize * (i + 1);
		array_alloc(a, total);
	}
	memcpy((char *)a->mem+i*a->dsize, d, a->dsize);
	if (a->count <= i) {
		a->count = i+1;
	}
}

void* array_get(array_t *a, int i)
{
	if (i >= a->count) abort();
	return (char*)a->mem + a->dsize*i;
}

int array_length(array_t *a)
{
	return a->count;
}

void array_del(array_t *a, int i)
{
	if (i >= a->count) abort();
	while (i+1 < a->count) {
		array_set(a, i, array_get(a, i+1));
		i++;
	}
	a->count--;
}

void array_shrink(array_t *a)
{
	int space, best;
	space = a->size / a->dsize;
	best = 0;
	while (1) {
		space /= 2;
		if (space > a->count) {
			best = space;
		} else {
			break;
		}
	}
	if (best) {
		if (best < ARRAY_INIT_COUNT) {
			best = ARRAY_INIT_COUNT;
		}
	}
	array_alloc(a, best*a->dsize);
}
