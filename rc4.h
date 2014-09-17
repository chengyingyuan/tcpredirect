#ifndef RC4_H
#define RC4_H
#include <stdint.h>

/**
 * A struct to keep state of RC4
 *
 */
typedef struct rc4 
{
	uint8_t ks[256]; // key stream
	int i;
	int j;

} rc4_t;

rc4_t* rc4_init(uint8_t *key, int klen);
void rc4_destroy(rc4_t *r);
uint8_t rc4_next(rc4_t *r);
void rc4_cipher(rc4_t *r, uint8_t *stream, int slen);

#endif