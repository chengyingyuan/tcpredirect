/**
 * Crypto algorithm based on rc4
 *
 * See http://en.wikipedia.org/wiki/RC4 for detail
 *
 */
#include "rc4.h"
#include <stdlib.h>

rc4_t* rc4_init(uint8_t *key, int klen)
{
	rc4_t *r;
	int i, j;
	uint8_t t;

	r = (rc4_t *)calloc(1, sizeof(rc4_t));
	r->i = r->j = 0;
	for (i = 0; i < 256; i++) {
		r->ks[i] = i;
	}
	j = 0;
	for (i = 0; i < 256; i++) {
		j = (j + r->ks[i] + key[i % klen]) % 256;
		t = r->ks[i];
		r->ks[i] = r->ks[j];
		r->ks[j] = t;
	}
	return r;
}

void rc4_destroy(rc4_t *r)
{
	free(r);
}

uint8_t rc4_next(rc4_t *r)
{
	uint8_t t;

	//calculate next value of i and j
	r->i = (r->i+1) % 256;
	r->j = (r->j + r->ks[r->i]) % 256;
	// swap r->ks[i] and r->ks[j]
	t = r->ks[r->i];
	r->ks[r->i] = r->ks[r->j];
	r->ks[r->j] = t;
	//generate next crypto char
	return r->ks[(r->ks[r->i] + r->ks[r->j]) % 256];
}

void rc4_cipher(rc4_t *r, uint8_t *stream, int slen)
{
	int i = 0;
	while (i < slen) {
		stream[i] = stream[i] ^ rc4_next(r);
		i++;
	}
}
