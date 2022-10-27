#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "headers/common.h"
#include "headers/algorithm.h"

// typedef struct{
//     uint32_t    size;
//     uint32_t    count;
//     uint64_t*   table;
// }array_t;


array_t* array_construct(int size){
    array_t* arr = malloc(sizeof(array_t));
    arr->count = 0;
    arr->size = size;
    arr->table = malloc(sizeof(uint64_t) * size);

    return arr;
}

void array_free(array_t* arr){
    free(arr->table);
    arr->table = NULL;
    free(arr);
    arr = NULL;
}

int array_insert(array_t** address,uint64_t value){
    array_t* arr = *address;
    if(arr == NULL){
        return 0;
    }

    if(arr->count == arr->size){
        // malloc to twice size
        uint64_t* old_table = arr->table;
        arr->size = arr->size * 2;
        arr->table = malloc(sizeof(uint64_t) * arr->size);
        // count unchanged

        // copy from old to new table
        for(int i=0; i<arr->size; ++i){
            arr->table[i] = old_table[i];
        }
        // free the old table memory
        free(old_table);
    }

    // insert the new value
    arr->table[arr->count] = value;
    arr->count++;

    return 1;
}

int array_delete(array_t* arr,int index){
    if(arr == NULL || (index < 0 || index >= arr->count)){
        return 0;
    }

    if((arr->count - 1) <= (arr->size / 4)){
        // shrink the table to half when payload is quater
        uint64_t* old_table = arr->table;

        arr->size = arr->size / 2;
        arr->table = malloc(sizeof(uint64_t) * arr->size);
        // count unchanged

        // copy from old to new table
        int j=0;
        for(int i=0; i<arr->count; ++i){
            if(i != index){
                arr->table[j] = old_table[i];
                j += 1;
            }
        }
        arr->count -= 1;
        free(old_table);
        return 1;
    }else{
        // copy move from index forward
        for(int i=index+1; i<arr->count; ++i){
            arr->table[i-1] = arr->table[i];
        }
        arr->count -= 1;
        return 1;
    }
    return 0;
}

int array_get(array_t* arr,int index,uint64_t* valptr){
    if(index >= arr->count){
        return 0;
    }else{
        *valptr = arr->table[index];
        return 1;
    }
    return 0;
}

void print_array(array_t* arr){
    if((DEBUG_VERBOSE_SET & DEBUG_DATASTRUCTURE) == 0){
        return;
    }
    printf("array size: %u count:%u\n",arr->size,arr->count);
    for(int i=0; i<arr->count; ++i){
        printf("\t[%d] %16lx\n",i,arr->table[i]);
    }
}