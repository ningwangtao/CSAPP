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
    st_entry_t*   dst;  // dst symbol: used for relocation - find the function
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

/**
 * @brief 
 * 
 * @param dst 
 * @param smap_table 
 * @param smap_count 
 */
static void compute_section_header(elf_t* dst,smap_t *smap_table,int *smap_count){
    // we only have .text, .rodata, .data as symbols in the section
    // .bss is not taking any section memory

    int count_text = 0;
    int count_rodata = 0;
    int count_data = 0;
    for(int i=0;i<*smap_count; ++i){
        st_entry_t *sym = smap_table[i].src;
        if(strcmp(sym->st_name,".text") == 0){
            count_text++;
        }else if(strcmp(sym->st_name,".rodata") == 0){
            count_rodata++;
        }else if(strcmp(sym->st_name,".data") == 0){
            count_data++;
        }
    }
    // TODO: count the section
    dst->sht_count = 0;
    // TODO: count the total lines
    dst->line_count = 0;
    
    // the target dst: line_count, sht_count, sht, .text, ,rodata, .data, .symtab
    // print to buffer
    sprintf(dst->buffer[0],"%d",dst->line_count);
    sprintf(dst->buffer[1],"%d",dst->sht_count);

    // compute the run-time address of the sections: compact in memory
    u_int64_t text_runtime_addr = 0x00400000;

    // TODO
    u_int64_t rodata_runtime_addr = 0;
    u_int64_t data_runtime_addr = 0;
    u_int64_t symtab_runtime_addr = 0;

    // write the section header table
    assert(dst->sht == NULL);
    dst->sht = malloc(dst->sht_count * sizeof(sh_entry_t));

    // write int .data, .rodata, .text order
    // TODO: the start of the offser
    uint64_t section_offset = 0;
    int j = 0;
    sh_entry_t* sh = NULL;
    if(count_text > 0){
        //get the pointer
        assert(j < dst->sht_count);
        sh = &(dst->sht[j]);

        // write the fields
        strcpy(sh->sh_name,".text");
        sh->sh_addr = text_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_text;

        // TODO: write to buffer
        sprintf(dst->buffer[0],"%s,0x%lx,%ld,%ld",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);

        // update the index
        j++;
        section_offset += sh->sh_size;
    }else if(count_rodata > 0){
        // TODO:
    }else if(count_data > 0){
        // TODO:
    }

    // TODO: symtab

    assert(j+1 == dst->sht_count);

    // print and check
    if((DEBUG_VERBOSE_SET & DEBUG_LINKER) == 0){
        for(int i=0;i<dst->sht_count;++i){
            printf("%s/n",dst->buffer[1+1+i]);
        }
    }
}


/**
 * @brief merge the target section lines from ELF files and update dst symtab
 * 
 * @param srcs 
 * @param num_srcs 
 * @param dst 
 * @param smap_table 
 * @param smap_count 
 */
static void merge_section(elf_t** srcs,int num_srcs,elf_t* dst,smap_t* smap_table,int* smap_count){
    int line_written = 1 + 1 + dst->sht_count;
    int symt_written = 0;

    for(int section_index = 0; section_index < dst->sht_count; ++section_index){
        // get the section by section ID
        sh_entry_t* target_sh = &dst->sht[section_index];

        // merge the section
        // scan every input ELF file
        for(int i=0; i<num_srcs; ++i){
            int src_section_index = -1;
            // scan every section in this elf
            for(int j=0; j<srcs[i]->sht_count; ++j){
                // TODO: check if this ELF srcs[i] contains the same section as target_sh
            }

            // check if we have found this target section from src ELF
            if(src_section_index == -1){
                // search for the next ELF
                continue;
            }else{
                // found the section in this ELF srcs[i]
                // check the symtab
                for(int j=0; j< srcs[i]->symt_count; ++j){
                    st_entry_t* sym = &srcs[i]->symt[j];

                    for(int k=0; k<*smap_count; ++k){
                        // scan the cached dst symbols to check
                        // if this symbol should be merged into this section
                        if(sym == smap_table[k].src){
                            // exactly the cached symbol
                            // TODO: copy this symbol from srcs[i].buffer into dst.buffer

                            // copy the symbol table entry from srcs[i].symt[j] to dst.symt[symt_writteb]
                            assert(symt_written < dst->symt_count);
                            // TODO: copy the entry
                            // TODO: update the new offset

                            // update the smap_table
                            // this will hep the
                            smap_table[k].dst = &dst->symt[symt_written];

                            // TODO: update the counter
                            symt_written += 0;
                            line_written += 0;
                        }
                    } // symbol srcs[i].symt[j] has been checked
                } // ELF file srcs[i] has been checked
            }
        } //dst.sht[section_index] has been merged from src ELFs
    } // all sections in dst has been merged
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
void link_elf(elf_t** srcs, int num_srcs, elf_t* dst){

    // reset the destination since it`s a new 
    memset(dst,0,sizeof(elf_t));
    // create the map table to connect the source and destination elf files symbol
    int smap_count = 0;
    smap_t smap_table[MAX_SYMBOL_MAP_LENGTH];

    // update the smap table - symbol proccessing
    symbol_processing(srcs,num_srcs,dst,(smap_t*)&smap_table,&smap_count);

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

    // compute dst Section Header nad write into buffer
    // UPDATE section header table: compute suntime address of each section
    // UPDATE buffer: EOF file header: file line count,section header table line count,section header table
    // compute running address of each section: .text, .rodata, .data, .symbol
    // eof starting from 0x00400000
    compute_section_header(dst,smap_table,&smap_count);

    // malloc the dst.symt
    dst->symt = malloc(dst->symt_count * sizeof(st_entry_t));

    // to this point, the EOF file header and section header table is palced
    // merge the left sections and relocate the entrirs in .text and .dsts

    // merge the symbol content fron ELF src into dst sectopns
    merge_section(srcs,num_srcs,dst,smap_table,&smap_count);

    // TODO: update dst.buffer for dst.symt
    // find the buffer offset for symbol table and write the buffer
}

// https://github.com/yangminz/bcst_csapp/commit/054da4950ebaa0866ce76e69021d7bfc15ace85a