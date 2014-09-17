#ifndef SLINK_H
#define SLINK_H

struct slink_node
{
	struct slink_node *next;
	void *data;
};

struct slink
{
	struct slink_node *head;
	struct slink_node *tail;
	int count;
};

typedef struct slink_node slink_node_t;
typedef struct slink slink_t;

slink_t *slink_init();
void slink_destroy(slink_t *s);
void slink_delete(slink_t *s, slink_node_t *node);
void slink_push(slink_t *s, void *p);
void* slink_pop(slink_t *s);
int slink_length(slink_t *s);
void slink_enqueue(slink_t *s, void *p);
void* slink_dequeue(slink_t *s);

#endif