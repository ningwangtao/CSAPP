#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "headers/common.h"
#include "headers/datastruct.h"

typedef struct{
    // tag is like the generation in GC
    // or it's like the mark process in mark-sweep algorithm
    // but we manually mark the memory block with tag instead
    // since C connot distinguish the pointer and data correctly
    uint64_t    tag;
    uint64_t    size;
    void*       ptr;
}tag_block_t;

// one draw back of this implementation is that the tag is dependent on linkedlist
// so linkedlist cannot use tag_*() functions
// and also cleanup events(array) cannot use tag_*()
static linkedlist_t* tag_list = NULL;

static uint64_t compute_tag(char* str){
    int p = 31;
    int m = 1000000007;

    int k = p;
    int v = 0;
    for(int i=0; i<strlen(str); ++i){
        v = (v + ((int)str[i] * k) % m) % m;
        k = (k * p) % m;
    }
    return v;
}

static void tag_destroy(){
    for(int i=0; i<tag_list->count; ++i){
        linkedlist_node_t* p = linkedlist_next(tag_list);
        tag_block_t* b = (tag_block_t*)p->value;

        // free heap memory
        free(b->ptr);
        // free block
        free(b);
        // free from the linked list
        linkedlist_delete(tag_list,p);
    }
    linkedlist_free(tag_list);
}

void* tag_malloc(uint64_t size, char* tagstr){
    uint64_t tag = compute_tag(tagstr);

    tag_block_t* b = malloc(sizeof(tag_block_t));
    b->tag = tag;
    b->size = size;
    b->ptr = malloc(size);

    // manage the block 
    if(tag_list == NULL){
        tag_list = linkedlist_construct();
        // remember to clean the tag_list finally
        add_cleanup_event(&tag_destroy);
    }
    // add the heap address to the managing list
    linkedlist_add(&tag_list,(uint64_t)b);

    return b->ptr;
}

int tag_free(void* ptr){
    int found = 0;

    // it's very slow because we are managing it
    for(int i=0; i<tag_list->count; ++i){
        linkedlist_node_t* p = linkedlist_next(tag_list);

        tag_block_t* b = (tag_block_t*)p->value;
        if(b->ptr == ptr){
            // found this block
            linkedlist_delete(tag_list,p);
            // free block
            free(b);
            found = 1;
            break;
        }
    }
    if(found == 0){
        // or we should exit the process at once?
        return 0;
    }
    free(ptr);
    return 1;
}

void tag_sweep(char* tagstr){
    // sweep all the memory with target tag
    // NOTE THAT THIS IS VERY DANGEROUSSINCE IT WILL FREE ALL TAG MEMORY
    // CALL THIS FUNCTION ONLY IF YOU ARE VERY CONFIDENT
    uint64_t tag = compute_tag(tagstr);

    for(int i=0; i<tag_list->count; ++i){
        linkedlist_node_t* p = linkedlist_next(tag_list);

        tag_block_t* b = (tag_block_t*)p->value;
        if(b->tag == tag){
            // free heap memory
            free(b->ptr);
            // free block
            free(b);
            // free from linked list
            linkedlist_delete(tag_list,p);
        }
    }
}
