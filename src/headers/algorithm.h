#ifndef DATASTRUCTURE_GUARD
#define DATASTRUCTURE_GUARD

#include <stdint.h>

/**************************************/
/*     Circular Double Linked List    */
/**************************************/
typedef struct LINKED_LIST_NODE_STRUCT{
    uint64_t    value;
    struct      LINKED_LIST_NODE_STRUCT* prev;
    struct      LINKED_LIST_NODE_STRUCT* next;
}linkedlist_node_t;

typedef struct{
    linkedlist_node_t*      head;
    uint64_t                count;
}linkedlist_t;

linkedlist_t* linkedlist_construct();
void linkedlist_free(linkedlist_t* list);
int linkedlist_add(linkedlist_t** address,uint64_t value);
int linkedlist_delete(linkedlist_t* list,linkedlist_node_t* node);
linkedlist_node_t* linkedlist_get(linkedlist_t* list,uint64_t value);
linkedlist_node_t* linkedlist_next(linkedlist_t* list);

/**************************************/
/*         Dynamic Array              */
/**************************************/
typedef struct{
    uint32_t    size;
    uint32_t    count;
    uint64_t*   table;
}array_t;

array_t* array_construct(int size);
void array_free(array_t* arr);
int array_insert(array_t** address,uint64_t value);
int array_delete(array_t* arr,int index);
int array_get(array_t* arr,int index,uint64_t* valptr);
void print_array(array_t* arr);

/**************************************/
/*         Trie - Prefix Tree         */
/**************************************/
typedef struct TRIE_NODE_STRUCT{
    // '0'-'9','a'-'z','%'
    int                         isvalid;
    uint64_t                    value;
    struct TRIE_NODE_STRUCT*    next[37];
}trie_node_t;

trie_node_t* trie_construct();
int trie_insert(trie_node_t** address,char* key,uint64_t value);
int trie_get(trie_node_t* root,char* key,uint64_t* valptr);
void trie_free(trie_node_t* root);
void trie_print(trie_node_t* root);

/**************************************/
/*     hashtable data structures      */
/**************************************/
typedef struct{
    int             localdepth;     // the local depth
    int             counter;        // the counter of slots (have data)
    char**          karray;
    uint64_t*       varray;
}hashtable_bucket_t;

typedef struct{ 
    int                     num;            // number of buckeys = 1 << globaldepth
    int                     globaldepth;    // the global depth

    int                     size;           // the size of (key,value) tuples of each bucket
    hashtable_bucket_t**    directory;   // the internal table is actually an array
}hashtable_t;

hashtable_t* hashtable_construct(int size);
void hashtable_free(hashtable_t* tab);
int hashtable_get(hashtable_t* tab,char* key,uint64_t* valptr);
int hashtable_insert(hashtable_t** address,char* key,uint64_t val);
void print_hashtable(hashtable_t* tab);

#endif