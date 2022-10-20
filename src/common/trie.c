#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "headers/cpu.h"
#include "headers/memory.h"
#include "headers/common.h"

void trie_insert(trie_node_t** root,char* key,uint64_t val){
    trie_node_t** p = root;
    for(int i=0; i<strlen(key); ++i){
        if(*p == NULL){
            *p = malloc(sizeof(trie_node_t));
        }
        p = &((*p)->next[(int)key[i]]);
    }
    if(*p == NULL){
        *p = malloc(sizeof(trie_node_t));
    }
    // may overwrite
    (*p)->address = val;
}
int trie_get(trie_node_t* root,char* key,uint64_t* val){
    trie_node_t* p = root;
    for(int i=0; i<strlen(key); ++i){
        if(p == NULL){
            // not found
            return 0;
        }
        p = p->next[(int)key[i]];
    }
    *val = p->address;
    return 1;
}

void trie_free(trie_node_t* root){
    // two ways
    // 1. like the mark-sweep algorithm
    // 2. recursive *
    printf("func into trie free\n");
    if(root == NULL){
        return ;
    }
    for(int i=0; i<128; ++i){
        trie_free(root->next[i]);
    }
    free(root);
}

static void trie_dfs_print(trie_node_t* x, int level, char c){
    if(x != NULL){
        if(level > 0){
            for(int i=0; i<level - 1; ++i){
                printf("\t");
            }
            printf("[%c] %p\n",c,x);
        }
        for(int j=0;j<128; ++j){
            trie_dfs_print(x->next[j],level+1,(char)j);
        }
    }
}

void trie_print(trie_node_t* root){
    if((DEBUG_VERBOSE_SET & DEBUG_PARSEINST) == 0){
        return;
    }
    if(root == NULL){
        printf("NULL\n");
    }else{
        printf("Trie tree: \n");
    }

    trie_dfs_print(root,0,0);
}