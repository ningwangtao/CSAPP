// include suards to prevent double declaration of any identifiers
// such ad types, enums and static cariables
#ifndef MEMORY_GUARD
#define MEMORY_GUARD

#include <stdint.h>
#include "headers/cpu.h"

/*=============================================*/
/*       physical memory on dram chips         */
/*=============================================*/

// physical memory space is decided by the physical address
// in this simulator, there are 4 + 4 + 6 = 16 bits physical address
// then the physical space is (1 << 16) = 65536
// total 16 physical memory
#define PHYSICAL_MEMORY_SPACE      65536
#define MAX_INDEX_PHYSICAL_PAGE    15

// physical memory
// 16 physical memory pages
uint8_t pm[PHYSICAL_MEMORY_SPACE];

/*=============================================*/
/*                   memory R/W                */
/*=============================================*/

// used by instructions: read or write uint64_t to DRAM
uint64_t read64bits_dram(uint64_t paddr);
void write64bits_dram(uint64_t paddr, uint64_t data);
void readinst_dram(uint64_t paddr, char* buf);
void writeinst_dram(uint64_t paddr, const char* str);


#endif