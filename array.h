#ifndef ARRAY_H
#define ARRAY_H

#define ARRAY_INIT_COUNT 2

struct array {
	void *mem;
	int size;
	int dsize;
	int count;
};

typedef struct array array_t;

array_t *array_init(int dsize, int init_count);
void array_destroy(array_t *a);
void array_put(array_t *a, void *d);
void array_set(array_t *a, int i, void *d);
void* array_get(array_t *a, int i);
int array_length(array_t *a);
void array_del(array_t *a, int i);
void array_shrink(array_t *a);

#endif
