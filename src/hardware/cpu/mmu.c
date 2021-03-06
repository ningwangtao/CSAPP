// Memory Management Unit
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "headers/cpu.h"
#include "headers/memory.h"

uint64_t va2pa(uint64_t vaddr){
//    return vaddr & (0xffffffffffffffff >> (64 - MAX_INDEX_PHYSICAL_PAGE));
    return vaddr % PHYSICAL_MEMORY_SPACE;
}