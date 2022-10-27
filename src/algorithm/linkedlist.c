#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "headers/algorithm.h"


// typedef struct LINKED_LIST_NODE_STRUCT{
//     uint64_t    value;
//     struct      LINKED_LIST_NODE_STRUCT* prev;
//     struct      LINKED_LIST_NODE_STRUCT* next;
// }linkedlist_node_t;

// typedef struct{
//     linkedlist_node_t*      head;
//     uint64_t                count;
// }linkedlist_t;


// constructor and destructor

linkedlist_t* linkedlist_construct(){
    linkedlist_t* list = malloc(sizeof(linkedlist_t));
    list->count = 0;
    list->head = NULL;

    return list;
}

void linkedlist_free(linkedlist_t* list){
    if(list == NULL){
        return;
    }

    // listptr is the STACK ADDRESS storing the referenced address
    for(int i=0; i<list->count; ++i){
        linkedlist_node_t* node = list->head;
        list->head = list->head->next;
        if(node == list->head){
            // only one element
            free(node);
            // do not update list->count during deleting
        }else{
            // at least two elements
            node->prev->next = node->next;
            node->next->prev = node->prev;
            free(node);
            // do not update list->count during deleting
        }
    }
    free(list);
    list = NULL;
}

int linkedlist_add(linkedlist_t** address,uint64_t value){
    linkedlist_t* list = *address;
    if(list == NULL){
        return 0;
    }

    if(list->count == 0){
        // create a new head
        list->head = malloc(sizeof(linkedlist_node_t));
        list->head->prev = list->head;
        list->head->next = list->head;
        list->head->value = value;
        list->count = 1;
    }else{
        // insert to tail to implement FIFO policy
        linkedlist_node_t* node = malloc(sizeof(linkedlist_node_t));
        node->value = value;
        node->next = list->head;
        node->prev = list->head->prev;
        node->next->prev = node;
        node->prev->next = node;
        list->count++;
    }
    return 1;
}

int linkedlist_delete(linkedlist_t* list,linkedlist_node_t* node){
    if(list == NULL){
        return 0;
    }

    // update the prev and next pointer
    // same for the only one node situation
    node->prev->next = node->next;
    node->next->prev = node->prev;
    // free the node managed by the list
    free(node);
    list->count--;

    if(list->count == 0){
        list->head = NULL;
    }
    return 1;
}

linkedlist_node_t* linkedlist_get(linkedlist_t* list,uint64_t value){
    if(list == NULL){
        return NULL;
    }

    linkedlist_node_t* p = list->head;

    for(int i=0; i<list->count;++i){
        if(p->value == value){
            return p;
        }
        p = p->next;
    }
    return NULL;
}

// traverse the linked list
linkedlist_node_t* linkedlist_next(linkedlist_t* list){
    if(list == NULL){
        return NULL;
    }
    list->head = list->head->next;
    return list->head->prev;
}