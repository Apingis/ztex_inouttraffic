
#define PKT_TYPE_WORD_LIST	1

// ***************************************************************
//
// Word List
//
// ***************************************************************

struct pkt *pkt_word_list_new(char **words);

struct pkt *pkt_word_list_new_fixed_len(char *words, int num_words, int max_len);
