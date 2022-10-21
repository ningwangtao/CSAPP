#ifndef DATASTRUCTURE_GUARD
#define DATASTRUCTURE_GUARD

#include <stdint.h>

/**************************************/
/*        trie data structures        */
/**************************************/
typedef struct TRIE_NODE_STRUCT{
    // '0'-'9','a'-'z','%'
    struct TRIE_NODE_STRUCT* next[37];
    uint64_t address;
}trie_node_t;

void trie_insert(trie_node_t** root,char* key,uint64_t val);
int trie_get(trie_node_t* root,char* key,uint64_t* val);
void trie_free(trie_node_t* root);
void trie_print(trie_node_t* root);

/**************************************/
/*     hashtable data structures      */
/**************************************/
typedef struct{
    int localdepth;     // the local depth
    int counter;        // the counter of slots (have data)
    char** karray;
    uint64_t* varray;
}hashtable_bucket_t;

typedef struct{ 
    int num;            // number of buckeys = 1 << globaldepth
    int globaldepth;    // the global depth

    int bsize;           // the size of (key,value) tuples of each bucket
    hashtable_bucket_t** barray;   // the internal table is actually an array
}hashtable_t;

hashtable_t* hashtable_construct(int bsize);
void hashtable_free(hashtable_t* tab);
int hashtable_get(hashtable_t* tab,char* key,uint64_t* val);
int hashtable_insert(hashtable_t** tab_addr,char* key,uint64_t val);
void print_hashtable(hashtable_t* tab);

#endif