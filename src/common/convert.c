#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "headers/common.h"



/**
 * @brief covert string to uint64_t
 * 
 * @param str 
 * @return uint64_t 
 */
uint64_t string2uint(const char* str){
    return string2uint_range(str,0,-1);
}


/**
 * @brief covert string to uint64_t from str[start] to str[end]
 * 
 * @param str source string
 * @param start start position
 * @param end  end position
 * @return uint64_t 
 */
uint64_t string2uint_range(const char* str,int start,int end){

    // start: starting index inclusive
    // end: ending index inclusive
    end = (end == -1) ? strlen(str) - 1 : end;

    uint64_t uval = 0;
    uint8_t sign_bit = 0; // 0 - positive ; 1 - negative

    // DFA: deterministic finite automate to scan string and get value
    uint8_t state = 0;

    for(int i=start; i<=end; ++i){
        char c = str[i];

        if(state == 0){
            if(c == '0'){
                state = 1;
                uval = 0;
                continue;
            }else if('1' <= c && c <= '9'){
                state = 2;
                uval += c - '0';
                continue;
            }else if(c == '-'){
                state = 3;
                sign_bit = 1;
                continue;
            }else if(c == ' ' || c == '\t' || c == '\r' || c == '\n'){
                // skip leading space
                state = 0;
                continue;
            }else{
                goto fail;
            }
        }else if(state == 1){
            // check zero
            if('0' <= c && c <= '9'){
                state = 2;
                uval = 10 * uval + c - '0';
                continue;
            }else if(c == 'x'){
                state = 4;
                continue;
            }else if(c == ' '){
                state = 6;
                continue;
            }else{
                goto fail;
            }
        }else if(state == 2){
            // dec number
            if('0' <= c && c <= '9'){
                state = 2;
                uint64_t pval = uval;
                uval = uval * 10 + c - '0';
                // meybe overflow
                if(pval > uval){
                    printf("(uint64_t)%s overflow: cannot convert\n",str);
                    goto fail;
                }
            }else if(c == ' '){
                state = 6;
                continue;
            }else{
                goto fail;
            }
        }else if(state == 3){
            // negative
            if(c == '0'){
                state = 1;
                continue;
            }else if('1' <= c && c <= '9'){
                state = 2;
                uval = c -'0';
                continue;
            }else{
                goto fail;
            }
        }else if(state == 4 ||state == 5){
            // hex number
            if('0' <= c && c <= '9'){
                state = 5;
                uint64_t pval = uval;
                uval = uval * 16 + c - '0';
                // maybe overflow
                if(pval > uval){
                    printf("(uint64_t)%s overflow: cannot convert\n",str);
                    goto fail;
                }
                continue;
            }else if('a' <= c && c <= 'f'){
                state = 5;
                uint64_t pval = uval;
                uval = uval * 16 + c - 'a' + 10;
                // maybe overflow
                if(pval > uval){
                    printf("(uint64_t)%s overflow: cannot convert\n",str);
                    goto fail;
                }
                continue;
            }else if(state == 5 && (c == ' ' || c == '\t' || c == '\r' || c == '\n')){
                state = 6;
                continue;
            }
            else{
                goto fail;
            }
        }else if(state == 6){
            if(c == ' ' || c == '\t' || c == '\r' || c == '\n'){
                // skip tailing spaces
                state = 6;
                continue;
            }else{
                goto fail;
            }
        }
    }
    if(sign_bit == 0){
        return uval;
    }else if(sign_bit == 1){
        if((uval & 0x8000000000000000) != 0){
            printf("(uint64_t)%s overflow: cannot convert\n",str);
            exit(0);
        }
        int64_t sv = -1 * (int64_t)uval;
        return *((uint64_t*)&sv);
    }

    fail:
    printf("type converter: <%s> connot be converted to integer\n",str);
    exit(0);
}

/**
 * @brief covert uint32_t to its float
 * 
 * @param u 
 * @return uint32_t 
 */
uint32_t uint2float(uint32_t u){
    if(u == 0x00000000){
        return 0x00000000;
    }

    // must be NORMALIZED 
    // counting the position of highest 1: u[n]
    int n = 31;
    while(0 <= n && (((u >> n) & 0x1) == 0x0)){
        n = n - 1;
    }
    uint32_t e, f;
    //    seee eeee efff ffff ffff ffff ffff ffff
    // <= 0000 0000 1111 1111 1111 1111 1111 1111
    if (u <= 0x00ffffff){
        // no need rounding
        uint32_t mask = 0xffffffff >> (32 - n);
        f = (u & mask) << (23 - n);
        e = n + 127;
        return (e << 23) | f;
    }
    // >= 0000 0001 0000 0000 0000 0000 0000 0000
    else{
        // need rounding
        // expand to 64 bit for situations like 0xffffffff
        uint64_t a = 0;
        a += u;
        // compute g, r, s
        uint32_t g = (a >> (n - 23)) & 0x1;     // Guard bit, the lowest bit of the result
        uint32_t r = (a >> (n - 24)) & 0x1;     // Round bit, the highest bit to be removed
        uint32_t s = 0x0;                       // Sticky bit, the OR of remaining bits in the removed part (low)
        for (int j = 0; j < n - 24; ++ j){
            s = s | ((u >> j) & 0x1);
        }
        // compute carry
        a = a >> (n - 23);
        // 0    1    ?    ... ?
        // [24] [23] [22] ... [0]
        /* Rounding Rules
            +-------+-------+-------+-------+
            |   G   |   R   |   S   |       |
            +-------+-------+-------+-------+
            |   0   |   0   |   0   |   +0  | round down
            |   0   |   0   |   1   |   +0  | round down
            |   0   |   1   |   0   |   +0  | round down
            |   0   |   1   |   1   |   +1  | round up
            |   1   |   0   |   0   |   +0  | round down
            |   1   |   0   |   1   |   +0  | round down
            |   1   |   1   |   0   |   +1  | round up
            |   1   |   1   |   1   |   +1  | round up
            +-------+-------+-------+-------+
        carry = R & (G | S) by K-Map
        */
        if ((r & (g | s)) == 0x1){
            a = a + 1;
        }
        // check carry
        if ((a >> 23) == 0x1){
            // 0    1    ?    ... ?
            // [24] [23] [22] ... [0]
            f = a & 0x007fffff;
            e = n + 127;
            return (e << 23) | f;
        }
        else if ((a >> 23) == 0x2){
            // 1    0    0    ... 0
            // [24] [23] [22] ... [0]
            e = n + 1 + 127;
            return (e << 23);
        }
    }
    // inf as default error
    return 0x7f800000;
}