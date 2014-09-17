#include "slink.h"
#include <stdlib.h>
#include <assert.h>

slink_t *slink_init()
{
	slink_t *s = (slink_t *)calloc(1, sizeof(slink_t));
	assert(s);
	s->count = 0;
	s->head = s->tail = NULL;
	return s;
}

void slink_destroy(slink_t *s)
{
	slink_node_t *curr = s->head;
	slink_node_t *next;
	while (curr != NULL) {
		next = curr->next;
		free(curr);
		curr = next;
	}
	free(s);
}

void slink_delete(slink_t *s, slink_node_t *node)
{
	slink_node_t *curr = s->head;
	slink_node_t *prev = NULL;
	while (curr && curr!=node) {
		prev = curr;
		curr = curr->next;
	}
	if (curr == node) {
		if (curr == s->head) {
			s->head = curr->next;
		}
		if (curr == s->tail) {
			s->tail = prev;
		}
		if (prev) {
			prev->next = curr->next;
		}
		free(node);
		s->count--;
	}
}

void slink_push(slink_t *s, void *p)
{
	slink_node_t *node = (slink_node_t *)calloc(1, sizeof(slink_node_t));
	node->data = p;
	node->next = s->head;
	s->head = node;
	if (!s->tail) {
		s->tail = node;
	}
	s->count++;
}

void* slink_pop(slink_t *s)
{
	slink_node_t *node = s->head;
	void *p = node->data;
	s->head = node->next;
	if (s->tail == node) {
		s->tail = NULL;
	}
	free(node);
	s->count--;
	return p;
}

int slink_length(slink_t *s)
{
	return s->count;
}

void slink_enqueue(slink_t *s, void *p)
{
	slink_node_t *node = (slink_node_t *)calloc(1, sizeof(slink_node_t));
	node->next = NULL;
	if (!s->tail) {
		s->tail = node;
	} else {
		s->tail->next = node;
		s->tail = node;
	}
	if (!s->head) {
		s->head = node;
	}
	s->count++;
}

void* slink_dequeue(slink_t *s)
{
	return slink_pop(s);
}
