#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkt_comm.h"
#include "word_list.h"

struct pkt *pkt_word_list_new(char **words)
{
	int len = 0;
	int i;
	for (i = 0; words[i]; i++) {
		len += strlen(words[i]) + 1;
	}
	if (!len) {
		pkt_error("pkt_wordlist_new(): empty packet\n");
		return NULL;
	}

	char *data = malloc(len);
	if (!data) {
		pkt_error("pkt_wordlist_new(): unable to allocate %d bytes\n", len);
		return NULL;
	}
	
	int offset = 0;
	for (i = 0; words[i]; i++) {
		strcpy(data + offset, words[i]);
		offset += strlen(words[i]) + 1;
	}

	struct pkt *pkt = pkt_new(PKT_TYPE_WORD_LIST, data, len);
	return pkt;
}

struct pkt *pkt_word_list_new_fixed_len(char *words, int num_words, int max_len)
{
	int len = 0;
	int i;
	for (i = 0; i < num_words; i++) {
		len += strnlen(words + i*max_len, max_len) + 1;
	}
	if (!len) {
		pkt_error("pkt_word_list_new_fixed_len(): empty packet\n");
		return NULL;
	}

	char *data = malloc(len);
	if (!data) {
		pkt_error("pkt_word_list_new_fixed_len(): unable to allocate %d bytes\n", len);
		return NULL;
	}

	int offset = 0;
	for (i = 0; i < num_words; i++) {
		int word_len = strnlen(words + i*max_len, max_len);
		memcpy(data + offset, words + i*max_len, word_len);
		offset += word_len;
		*(data + offset++) = 0;
	}
	
	struct pkt *pkt = pkt_new(PKT_TYPE_WORD_LIST, data, len);
	return pkt;	
}
