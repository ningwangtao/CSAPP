#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "headers/common.h"
#include "headers/cpu.h"
#include "headers/memory.h"

uint64_t hash_function(char* str){
    int p= 31;
    int m = 1000000007;

    int k = p;
    int v = 0;
    for(int i=0; i<strlen(str); ++i){
        v = (v + ((int)str[i] * p) % m) % m;
        k = (k * p) % m;
    }
    return v;
}

typedef struct{
    int localdepth;     // the local depth
    int counter;        // the counter of slots (have data)
    char** karray;
    uint64_t* varray;
}bucket_t;

typedef struct{
    int size;           // the size of (key,value) tuples of each bucket
    int num;            // number of buckeys = 1 << globaldepth
    int globaldepth;    // the global depth

    bucket_t* barray;   // the internal table is actually an array
}hashtable_t;

// the constructor
hashtable_t* hashtable(){
    hashtable_t* tab = malloc(sizeof(hashtable_t));

    tab->globaldepth = 1;
    tab->num = 1 << tab->globaldepth;
    tab->size = 4;

    tab->barray = malloc(sizeof(bucket_t) * tab->size);
    for(int i=0; i<tab->size; ++i){
        bucket_t* b = &tab->barray[i];

        b->localdepth = 1;
        b->counter = 0;
        b->karray = malloc(tab->size * sizeof(char*));
        b->varray = malloc(tab->size * sizeof(uint64_t));
    }
    return tab;
}

int hashtable_get(hashtable_t* tab,char* key,uint64_t* val){
    uint64_t hashkey = hash_function(key);
    int mask_bits_low = 64 - tab->globaldepth;
    uint64_t hid = (hashkey << mask_bits_low) >> mask_bits_low;

    bucket_t* b = &tab->barray[hid];
    for(int i=0; i<b->counter; ++i){
        if(strcmp(b->karray[i],key) == 0){
            // found
            *val = b->varray[i];
            return 1;
        }
    }
    // not found 
    return 0;
}

int hashtable_insert(hashtable_t* tab,char* key,uint64_t* val){
    uint64_t hashkey = hash_function(key);
    int mask_bits_low = 64 - tab->globaldepth;
    uint64_t hid = (hashkey << mask_bits_low) >> mask_bits_low;

    bucket_t* b = &tab->barray[hid];
    if(b->counter < tab->size){
        // existing empty slot for inserting
        // insert without splitting
        b->karray[b->counter] = malloc(sizeof(char) * (1 + strlen(key)));
        strcpy(b->karray[b->counter],key);

        // store the reference of value
        b->varray[b->counter] = *val;
        b->counter += 1;
    }else{
        // full for this bucket's k-v array, expending the whole table
        return 0;
    }
    return 1;
}