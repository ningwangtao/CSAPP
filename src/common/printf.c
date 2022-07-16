#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include "headers/common.h"

/**
 * @brief wrapper of stdio printf
 * 
 * @param open_set controlled by the debug verbose set
 * @param format 
 * @param ... 
 * @return uint64_t 
 *         0x0: success
 *         0x1: no need debug info
 */
uint64_t debug_printf(uint64_t open_set, const char* format, ...){

    if((open_set & DEBUG_VERBOSE_SET) == 0x0){
        return 0x1;
    }

    //implementation of std printf()
    va_list argptr;
    va_start(argptr,format);
    vfprintf(stderr,format,argptr);
    va_end(argptr);

    return 0x0;
}