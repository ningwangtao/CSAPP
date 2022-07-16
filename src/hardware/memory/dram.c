// Dynamic Random Access Memory
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "headers/common.h"
#include "headers/cpu.h"
#include "headers/memory.h"

/*  
Be careful with the x86-64 little endian integer encoding
e.g. write 0x00007fd357a02ae0 to cache, the memory lapping should be:
    e0 2a a0 57 d3 7f 00 00
*/

// memory accessing used in instruction
/**
 * @brief 
 * 
 * @param paddr pysical
 * @param cr core index
 * @return uint64_t 
 */
uint64_t read64bits_dram(uint64_t paddr){
    if(DEBUG_ENABLE_SRAM_CACHE == 1){
        // try to read uint64_t from SRAM cache
        // little-endian
    }else{
        // read from DRAM directly
        // little-endian
        uint64_t val = 0x0;

        val += (((uint64_t)pm[paddr + 0]) << 0);
        val += (((uint64_t)pm[paddr + 1]) << 8);
        val += (((uint64_t)pm[paddr + 2]) << 16);
        val += (((uint64_t)pm[paddr + 3]) << 24);
        val += (((uint64_t)pm[paddr + 4]) << 32);
        val += (((uint64_t)pm[paddr + 5]) << 40);
        val += (((uint64_t)pm[paddr + 6]) << 48);
        val += (((uint64_t)pm[paddr + 7]) << 56);

        return val;
    }
    return 0x0;
}

/**
 * @brief 
 * 
 * @param paddr pysical address
 * @param data 
 * @param cr core index
 */
void write64bits_dram(uint64_t paddr, uint64_t data){
    if(DEBUG_ENABLE_SRAM_CACHE == 1){
        // try to write uint64_t from SRAM cache
        // little-endian
    }else{
        // write from DRAM directly
        // little-endian
        pm[paddr + 0] = (data >> 0) & 0xff;
        pm[paddr + 1] = (data >> 8) & 0xff;
        pm[paddr + 2] = (data >> 16) & 0xff;
        pm[paddr + 3] = (data >> 24) & 0xff;
        pm[paddr + 4] = (data >> 32) & 0xff;
        pm[paddr + 5] = (data >> 40) & 0xff;
        pm[paddr + 6] = (data >> 48) & 0xff;
        pm[paddr + 7] = (data >> 56) & 0xff;
    }
}

/**
 * @brief 
 * 
 * @param paddr 
 * @param buf 
 * @param cr 
 */
void readinst_dram(uint64_t paddr, char* buf){
    for(int i=0; i< MAX_INSTRUCTION_CHAR; ++i){
        buf[i] = (char)pm[paddr + i];
    }
}

/**
 * @brief 
 * 
 * @param paddr 
 * @param str 
 * @param cr 
 */
void writeinst_dram(uint64_t paddr, const char* str){
    uint8_t len = strlen(str);
    assert(len < MAX_INSTRUCTION_CHAR);

    for(uint8_t i=0;i<MAX_INSTRUCTION_CHAR; ++i){
        if(i < len){
            pm[paddr + i] = (uint8_t)str[i];
        }else{
            pm[paddr + i] = 0;
        }
    }
}