#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "headers/linker.h"
#include "headers/common.h"


#define MAX_SYMBOL_MAP_LENGTH         (64)
#define MAX_SECTION_BUFFER_LENGTH     (64)


// internal mapping between source and destination synbol entries
typedef struct{
    elf_t*        elf;   // source elf file
    st_entry_t*   src;   // source symbol
    st_entry_t*   dest;  // dst symbol: used for relocation - find the function
}smap_t;

/**
 * @brief 
 * 
 * @param sym 
 * @return uint8_t 
 */
static inline uint8_t symbol_precefence(st_entry_t* sym){
    // use inline function to improve efficiency in run-time by preprocessing
    // get the precedence for each symbol
    /*  we do not consider weak because it's very rare
        and we do not consider local because it's not conflicting
            bind        type        shndx               prec
            --------------------------------------------------
            global      notype      undef               0 - undefined        
            global      object      common              1 - tentative
            global      object      data,bss,rodata     2 - defined
            global      func        text                2 - defined
    */
    assert(sym->bind == STB_GLOBAL);
    if(strcmp(sym->st_shndx,"SHN_UNDEF") == 0 && sym->type == STT_NOTYPE){
        // Undefined: symbols referenced but not assigned a storage address
        return 0;
    }
    if(strcmp(sym->st_shndx,"COMMON") == 0 && sym->type == STT_OBJECT){
        // Tentative: section to be decided after symbol resolution
        return 1;
    }
    if( (strcmp(sym->st_shndx,".text") == 0 && sym->type == STT_FUNC) ||
        (strcmp(sym->st_shndx,".data") == 0 && sym->type == STT_OBJECT) ||
        (strcmp(sym->st_shndx,".rodata") == 0 && sym->type == STT_OBJECT) ||
        (strcmp(sym->st_shndx,".bss") == 0 && sym->type == STT_OBJECT) ){
        // Defined
        return 2;
    }

    debug_printf(DEBUG_LINKER,"symbol resolution: cannot determine the symbol \"%s\" precedence",sym->st_name);
    exit(1);
}

/**
 * @brief 
 * 
 * @param src 
 * @param map 
 */
static void simple_resolution(st_entry_t* sym, elf_t* sym_elf, smap_t* candidate){
    // sym: symbol from current elf file
    // candidate: pointer to the internal map table slot: src -> dst

    // determines which symbol is the one to be kept with 3 rules
    // rule 1: multiple strong symbols with the same name are not allowed
    // rule 2: given a strong symbol and multiple weak symbols with the same name, choose the strong symbol
    // rule 3: given multiple weak symbols with the same name, choose any of the weak symbols
    uint8_t pre1 = symbol_precefence(sym);
    uint8_t pre2 = symbol_precefence(candidate->src);

    if(pre1 == 2 && pre2 == 2){
        /* rule 1
                pre1     pre2
            ----------------------
                 2         2
        */
        debug_printf(DEBUG_LINKER,"symbol resolution: strong synbol \"%s\" is recedenced\n",sym->st_name);
        exit(1);
    }else if(pre1 != 2 && pre2 != 2){
        /* rule 3
                pre1     pre2
            ----------------------
                 0         0
                 0         1
                 1         0
                 1         1
        */
        if(pre1 > pre2){
            // use stronger one as best match
            candidate->src = sym;
            candidate->elf = sym_elf;
        }else{
            // keep default, do not change
        }
    }else if(pre1 == 2){
        /* rule 2
                pre1     pre2
            ----------------------
                 2         0
                 2         1
        */
        // select sym as best match
        candidate->src = sym;
        candidate->elf = sym_elf;
    }else{
        /* rule 2
                pre1     pre2
            ----------------------
                 0         2
                 1         2
        */
        // keep default, do not change
    }
}


/**
 * @brief 
 * 
 * @param src 
 * @param num_elf 
 * @param dst 
 * @param map 
 * @param smap_count 
 */
static void symbol_processing(elf_t** src,int num_elf,elf_t* dst, smap_t* smap_table, int *smap_count){

    // for every elf files
    for(int i=0;i<num_elf;++i){
        elf_t* elfp = src[i];

        // for every symbol from this elf file
        for(uint8_t j=0;j<elfp->symt_count;++j){
            st_entry_t* sym = &(elfp->symt[j]);
            if(sym->bind == STB_LOCAL){
                // insert the static (local) symbol to new elf qith confidence:
                // compiler would check if the symbol is redeclared in one *.c file
                assert(*smap_count < MAX_SYMBOL_MAP_LENGTH);
                // even if local symbol has the same name, just insert into dst
                smap_table[*smap_count].src = sym;
                smap_table[*smap_count].elf = elfp;
                // we have not created dst here
                (*smap_count)++;
            }else if(sym->bind == STB_GLOBAL){
                // for this bind: STB_GLOBAL, it's possible to have name conflict
                // check if this symbol has been cached in the map
                for(int k=0;k<*smap_count;++k){
                    // check name conflict
                    // what if the caches symbol is STB_LOCAL?
                    st_entry_t* candidate = smap_table[k].src;
                    if(candidate->bind == STB_GLOBAL && (strcmp(candidate->st_name,sym->st_name) == 0)){
                        // having name conflict, do simple symbol resolution
                        // pick one symbol from current sym and cached map[k]
                        simple_resolution(sym,elfp,&smap_table[k]);
                        goto NEXT_SYMBOL_PROCESS;
                    }
                }

                // not fine any name conflict
                // cache surrent symbol sym to the map since there is no name conflict
                assert(*smap_count <= MAX_SYMBOL_MAP_LENGTH);
                // update map table
                smap_table[*smap_count].src = sym;
                smap_table[*smap_count].elf = elfp;
                // we have not created dst here
                (*smap_count)++;
            }else if(sym->bind == STB_WEAK){
                // discare this situation, nothing to do 
            }
            NEXT_SYMBOL_PROCESS:
            // do nothing
            ;
        }
    }

    // all elf files have been processed
    // cleanup: check if there is any undefined symbol in the map table
    for(uint8_t i=0;i<*smap_count;++i){
        st_entry_t* s = smap_table[i].src;
        // check SHN_UNDEF here
        assert(strcmp(s->st_shndx,"SHN_UNDEF") != 0);
        assert(s->type != STT_NOTYPE);

        // the remaining COMMON go to .bss
        if(strcmp(s->st_shndx,"COMMON") == 0){
            char* bss = ".bss";
            for(int j=0;j<MAX_CHAR_SYMBOL_NAME;++j){
                if(j<4){
                    s->st_shndx[j] = bss[j];
                }else{
                    s->st_shndx[j] = '\0';
                }
            }
            s->st_value = 0;
        }
    }
}



/* ------------------------------------- */
/*  Exposed Interface for Static Linking */
/* ------------------------------------- */
/**
 * @brief 
 * 
 * @param src 
 * @param num_src 
 * @param dst 
 */
void link_elf(elf_t** src, int num_src, elf_t* dst){

    // reset the destination since it`s a new 
    memset(dst,0,sizeof(elf_t));
    // create the map table to connect the source and destination elf files symbol
    int smap_count = 0;
    smap_t smap_table[MAX_SYMBOL_MAP_LENGTH];

    // update the smap table - symbol proccessing
    symbol_processing(src,num_src,dst,(smap_t*)&smap_table,&smap_count);

    printf("--------------------------\n");
    for (int i = 0; i < smap_count; ++ i){
        st_entry_t *ste = smap_table[i].src;
        debug_printf(DEBUG_LINKER, "%s\t%d\t%d\t%s\t%d\t%d\n",
            ste->st_name,
            ste->bind,
            ste->type,
            ste->st_shndx,
            ste->st_value,
            ste->st_size);
    }

    // TODO: compute dst Section Header Table and write into buffer

    // TODO: merge the symbol content fron ELF src into dst sectopns
}