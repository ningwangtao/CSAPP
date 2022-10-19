#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "headers/linker.h"
#include "headers/common.h"


#define MAX_SYMBOL_MAP_LENGTH         (64)
#define MAX_SECTION_BUFFER_LENGTH     (64)
#define MAX_RELOCATION_LINES          (64)


// internal mapping between source and destination synbol entries
typedef struct{
    elf_t*        elf;   // source elf file
    st_entry_t*   src;   // source symbol
    st_entry_t*   dst;  // dst symbol: used for relocation - find the function
}smap_t;

/**************************************/
/*           Symbol Processing        */
/**************************************/
static void simple_resolution(st_entry_t* sym, elf_t* sym_elf, smap_t* candidate);
static void symbol_processing(elf_t** src,int num_elf,elf_t* dst, smap_t* smap_table, int *smap_count);

/**************************************/
/*           Section Merging          */
/**************************************/
static void merge_section(elf_t** srcs,int num_srcs,elf_t* dst,smap_t* smap_table,int* smap_count);
static void compute_section_header(elf_t* dst,smap_t *smap_table,int *smap_count);

/**************************************/
/*           Relocation               */
/**************************************/
static void relocation_processing(elf_t** srcs,int num_srcs,elf_t* dst,smap_t* smap_table,int* smap_count);
static void R_X86_64_32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced);
static void R_X86_64_PC32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced);
static void R_X86_64_PLT32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced);
typedef void (*rela_handler_t)(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced);
static rela_handler_t handler_table[3] = {
    &R_X86_64_32_handler,       // 0
    &R_X86_64_PC32_handler,     // 1
    /* linux commit b21ebf2: x86: Treat R_X86_64_PLT32 as R_X86_64_PC32*/
    &R_X86_64_PLT32_handler,    // 2
};

/**************************************/
/*           Helper function          */
/**************************************/
static const char* get_stb_string(st_bind_t bind);
static const char* get_stt_string(st_type_t type);
static inline uint8_t symbol_precefence(st_entry_t* sym);


/* ------------------------------------- */
/*  Exposed Interface for Static Linking */
/* ------------------------------------- */
/**
 * @brief Interface for Static Linking
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

    printf("link_elf--------------------------link_elf\n");
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
    dst->symt_count = smap_count;
    dst->symt = malloc(dst->symt_count * sizeof(st_entry_t));

    // to this point, the EOF file header and section header table is palced
    // merge the left sections and relocate the entrirs in .text and .dsts

    // merge the symbol content fron ELF src into dst sectopns
    merge_section(srcs,num_srcs,dst,smap_table,&smap_count);

    printf("-----------------------------\n");
    printf("after merging the sections:\n");
    for(int i=0; i< dst->line_count; ++i){
        printf("%s\n",dst->buffer[i]);
    }

    // UPDATE buffer: relocate the referencing in buffer
    // relocating: update the relocaation entries from ELF files into EOF buffer
    relocation_processing(srcs,num_srcs,dst,smap_table,&smap_count);

    // finally: check the EOF file
    if((DEBUG_LINKER & DEBUG_VERBOSE_SET) != 0){
        printf("----\nfinal output EOF:\n");
        for(int i=0; i< dst->line_count; ++i){
            printf("%s\n",dst->buffer[i]);
        }
    }
}


/**
 * @brief Get the stb string object
 * 
 * @param bind 
 * @return const char* 
 */
static const char* get_stb_string(st_bind_t bind){
    switch (bind)
    {
    case STB_GLOBAL:
        return "STB_GLOBAL";
    case STB_LOCAL:
        return "STB_LOCAL";
    case STB_WEAK:
        return "STB_WEAK";
    default:
        printf("incorrect symbol bind\n");
        exit(0);
    }
}

/**
 * @brief Get the stt string object
 * 
 * @param type 
 * @return const char* 
 */
static const char* get_stt_string(st_type_t type){
    switch (type)
    {
    case STT_NOTYPE:
        return "STT_NOTYPE";
    case STT_OBJECT:
        return "STT_OBJECT";
    case STT_FUNC:
        return "STT_FUNC";
    default:
        printf("incorrect symbol type\n");
        exit(0);
    }
}

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
        if(strcmp(sym->st_shndx,".text") == 0){
            count_text += sym->st_size;
        }else if(strcmp(sym->st_shndx,".rodata") == 0){
            count_rodata += sym->st_size;
        }else if(strcmp(sym->st_shndx,".data") == 0){
            count_data += sym->st_size;
        }
    }
    // count the section: with .symtab
    dst->sht_count = (count_text != 0) + (count_rodata != 0) + (count_data != 0) + 1;
    // count the total lines
    dst->line_count = 1 + 1 + dst->sht_count + count_text + count_rodata + count_data + *smap_count;
    
    // the target dst: line_count, sht_count, sht, .text, ,rodata, .data, .symtab
    // print to buffer
    sprintf(dst->buffer[0],"%d",dst->line_count);
    sprintf(dst->buffer[1],"%d",dst->sht_count);

    // compute the run-time address of the sections: compact in memory
    u_int64_t text_runtime_addr = 0x00400000;

    // 
    u_int64_t rodata_runtime_addr = text_runtime_addr + count_text * MAX_INSTRUCTION_CHAR * sizeof(char);
    u_int64_t data_runtime_addr = rodata_runtime_addr + count_rodata * sizeof(uint64_t);
    u_int64_t symtab_runtime_addr = 0; // for EOF, .symtab is not loaded into run-time memory but still on disk

    // write the section header table
    assert(dst->sht == NULL);
    dst->sht = malloc(dst->sht_count * sizeof(sh_entry_t));

    // write in .data, .rodata, .text order
    // the start of the offset
    uint64_t section_offset = 1 + 1 + dst->sht_count;
    int sh_index = 0;
    sh_entry_t* sh = NULL;
    // .text
    if(count_text > 0){
        //get the pointer
        assert(sh_index < dst->sht_count);
        sh = &(dst->sht[sh_index]);

        // write the fields
        strcpy(sh->sh_name,".text");
        sh->sh_addr = text_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_text;

        // write to buffer
        sprintf(dst->buffer[2 + sh_index],"%s,0x%lx,%ld,%ld",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);

        // update the index
        sh_index++;
        section_offset += sh->sh_size;
    }
    // .rodata
    if(count_rodata > 0){
        // get the pointer
        assert(sh_index < dst->sht_count);
        sh = &(dst->sht[sh_index]);

        // write the fields
        strcpy(sh->sh_name,".rodata");
        sh->sh_addr = rodata_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_rodata;

        // write to buffer
        sprintf(dst->buffer[2 + sh_index],"%s,0x%lx,%ld,%ld",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);

        // update the index
        sh_index++;
        section_offset += sh->sh_size;
    }
    // .data
    if(count_data > 0){
        // get the pointer
        assert(sh_index < dst->sht_count);
        sh = &(dst->sht[sh_index]);

        // write the fields
        strcpy(sh->sh_name,".data");
        sh->sh_addr = data_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_data;

        // write to buffer
        sprintf(dst->buffer[2 + sh_index],"%s,0x%lx,%ld,%ld",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);

        // update the index
        sh_index++;
        section_offset += sh->sh_size;
    }

    // .symtab
    assert(sh_index < dst->sht_count);
    sh = &(dst->sht[sh_index]);

    // write the fields
    strcpy(sh->sh_name,".symtab");
    sh->sh_addr = symtab_runtime_addr;
    sh->sh_offset = section_offset;
    sh->sh_size = *smap_count;

    sprintf(dst->buffer[2 + sh_index],"%s,0x%lx,%ld,%ld",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);

    assert(sh_index + 1 == dst->sht_count);

    // print and check
    if((DEBUG_VERBOSE_SET & DEBUG_LINKER) != 0){
        printf("compute_section_header------------compute_section_header\n");
        printf("Destination ELF's SHT in Buffer:\n");
        for(int i=0;i<2 + dst->sht_count;++i){
            printf("%s\n",dst->buffer[i]);
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
    int sym_section_offset = 0;

    debug_printf(DEBUG_LINKER,"merge_section, line_written = %d\n",line_written);

    for(int section_index = 0; section_index < dst->sht_count; ++section_index){
        // merge by the dst.sht order in symbol unit

        // get the section by section ID
        sh_entry_t* target_sh = &dst->sht[section_index];
        sym_section_offset = 0;
        debug_printf(DEBUG_LINKER,"merging section '%s'\n",target_sh->sh_name);

        // merge the section
        // scan every input ELF file
        for(int i=0; i<num_srcs; ++i){
            debug_printf(DEBUG_LINKER,"\tfrom source elf [%d]\n",i);
            int src_section_index = -1;
            // scan every section in this elf
            for(int j=0; j<srcs[i]->sht_count; ++j){
                // check if this ELF srcs[i] contains the same section as target_sh
                if(strcmp(target_sh->sh_name,srcs[i]->sht[j].sh_name) == 0){
                    // we have found the same section name
                    src_section_index = j;
                    // break 
                }
            }

            // check if we have found this target section from src ELF
            if(src_section_index == -1){
                // search for the next ELF
                // bacause the current ELF src[i] does not contain the target_sh
                continue;
            }else{
                // found the section in this ELF srcs[i]
                // check the symtab
                for(int j=0; j< srcs[i]->symt_count; ++j){
                    st_entry_t* sym = &srcs[i]->symt[j];
                    if(strcmp(target_sh->sh_name,sym->st_shndx) == 0){
                        for(int k=0; k<*smap_count; ++k){
                            // scan the cached dst symbols to check
                            // if this symbol should be merged into this section
                            if(sym == smap_table[k].src){
                                // exactly the cached symbol
                                debug_printf(DEBUG_LINKER,"\t\tsymbol '%s'\n",sym->st_name);

                                // this symbol should be merged into dst's section target_sh
                                // copy this symbol from srcs[i].buffer into dst.buffer
                                // srcs[i].buffer[sh_offset + st_value,sh_offset + st_value + st_size] inclusive
                                for(int t=0; t<sym->st_size; ++t){
                                    int dst_index = line_written + t;
                                    int src_index = srcs[i]->sht[src_section_index].sh_offset + sym->st_value + t;

                                    assert(dst_index < MAX_ELF_FILE_LENGTH);
                                    assert(src_index < MAX_ELF_FILE_LENGTH);

                                    strcpy(dst->buffer[dst_index],srcs[i]->buffer[src_index]);
                                }
                                // copy the symbol table entry from srcs[i].symt[j] to dst.symt[symt_written]
                                assert(symt_written < dst->symt_count);
                                // copy the entry
                                strcpy(dst->symt[symt_written].st_name,sym->st_name);
                                dst->symt[symt_written].bind = sym->bind;
                                dst->symt[symt_written].type = sym->type;
                                strcpy(dst->symt[symt_written].st_shndx,sym->st_shndx);
                                //MUST NOT BE A COMMON, so the section offset MUST NOT BE alignment
                                dst->symt[symt_written].st_value = sym_section_offset;
                                dst->symt[symt_written].st_size = sym->st_size;

                                // update the smap_table
                                // this will hep the relocation
                                smap_table[k].dst = &dst->symt[symt_written];

                                // update the counter
                                symt_written += 1;
                                line_written += sym->st_size;
                                sym_section_offset += sym->st_size;
                            }
                        } // symbol srcs[i].symt[j] has been checked
                    }
                } // all symbol in ELF file srcs[i] has been checked
            }
        } //dst.sht[section_index] has been merged from src ELFs
    } // all sections in dst has been merged

    // finally, merge .symtab
    for(int i=0; i < dst->symt_count; ++i){
        st_entry_t* sym = &dst->symt[i];
        sprintf(dst->buffer[line_written],"%s,%s,%s,%s,%ld,%ld",
            sym->st_name,get_stb_string(sym->bind),get_stt_string(sym->type),
            sym->st_shndx,sym->st_value,sym->st_size);
        line_written ++;
    }
    assert(line_written == dst->line_count);
}

/**
 * @brief 
 * 
 * @param srcs 
 * @param num_srcs 
 * @param dst 
 * @param smap_table 
 * @param smap_count 
 */
static void relocation_processing(elf_t** srcs,int num_srcs,elf_t* dst,smap_t* smap_table,int* smap_count){

    sh_entry_t* eof_text_sh = NULL;
    sh_entry_t* eof_data_sh = NULL;
    for(int i=0; i<dst->sht_count; ++i){
        if(strcmp(dst->sht[i].sh_name,".text") == 0){
            eof_text_sh = &(dst->sht[i]);
        }else if(strcmp(dst->sht[i].sh_name,".data") == 0){
            eof_data_sh = &(dst->sht[i]);
        }
    }

    // update the relocation entries: r_row, r_col, sym
    for(int i=0; i<num_srcs; ++i){
        elf_t* elf = srcs[i];

        // .rel.text
        for(int j=0; j<elf->reltext_count; ++j){
            rel_entry_t* r = &elf->reltext[j];

            // search the referencing symbol
            for(int k=0; k<elf->symt_count; ++k){
                st_entry_t* sym = &elf->symt[k];

                if(strcmp(sym->st_shndx,".text") == 0){
                    // must be referenced by a .text symbol
                    // TODO: check if this symbol is the one referencing
                    int sym_text_start = sym->st_value;
                    int sym_text_end = sym->st_value + sym->st_size - 1;

                    if(sym_text_start <= r->r_row && r->r_row <= sym_text_end){
                        // symt[k] is referencing reltext[j].sym
                        // search the smap table to find the EOF location
                        int smap_found = 0;
                        for(int t=0; t<*smap_count; ++t){
                            if(smap_table[t].src == sym){
                                smap_found = 1;
                                st_entry_t* eof_referencing = smap_table[t].dst;

                                // search the being referenced symbol
                                for(int u=0; u<*smap_count; ++u){
                                    // what is the EOF symbol name?
                                    // how to get the referenced symbol name
                                    if(strcmp(elf->symt[r->sym].st_name,smap_table[u].dst->st_name) == 0 && 
                                        smap_table[u].dst->bind == STB_GLOBAL){
                                        // sill now, the referencing row and referenced row are all found
                                        // update the location
                                        st_entry_t* eof_referenced = smap_table[u].dst;
                                        (handler_table[(int)r->type])(
                                            dst,eof_text_sh,r->r_row - sym->st_value + eof_referencing->st_value,
                                            r->r_col,
                                            r->r_addrend,
                                            eof_referenced
                                        );
                                        goto NEXT_REFERENCE_IN_TEXT;
                                    }
                                }
                            }
                        }
                        // referencing must be in smap_table
                        // because it has definition, is astrong symbol
                        assert(smap_found == 1);
                    }
                }
            }
            NEXT_REFERENCE_IN_TEXT:
            ;
        }

        // TODO: .rel.data
        for(int j=0; j<elf->reldata_count; ++j){
            rel_entry_t* r = &elf->reldata[j];

            // search the referencing symbol
            for(int k=0;k<elf->symt_count; ++k){
                st_entry_t* sym = &elf->symt[k];

                if(strcmp(sym->st_shndx,".data") == 0){
                    // must be referenced by a .data symbol
                    // check if this symbol is the one referencing
                    int sym_data_start = sym->st_value;
                    int sym_data_end = sym->st_value + sym->st_size - 1;

                    if(sym_data_start <= r->r_row && r->r_row <= sym_data_end){
                        // symt[k] is referencing reldata[j].sym
                        // search the smap table to find the EOF location
                        int smap_found = 0;
                        for(int t=0; t<*smap_count; ++t){
                            if(smap_table[t].src == sym){
                                smap_found = 1;
                                st_entry_t* eof_referencing = smap_table[t].dst;

                                // search the being referenced symbol
                                for(int u=0; u<*smap_count; ++u){
                                    // what is the EOF symbol name?
                                    // how to gei the referenced symbol name
                                    if(strcmp(elf->symt[r->sym].st_name,smap_table[u].dst->st_name) == 0 &&
                                        smap_table[u].dst->bind == STB_GLOBAL){
                                        // till now, the referencing row and referenced row are all found
                                        // update the location
                                        st_entry_t* eof_referenced = smap_table[u].dst;

                                        (handler_table[(int)r->type])(
                                            dst,eof_data_sh,
                                            r->r_row - sym->st_value + eof_referencing->st_value,
                                            r->r_col,
                                            r->r_addrend,
                                            eof_referenced
                                        );
                                        goto NEXT_REFERENCE_IN_DATA;
                                    }
                                }
                            }
                        }
                        // referencing must be in smap_table
                        // because it has definition, is a strong symbol
                        assert(smap_found == 1);
                    }
                }
            }
            NEXT_REFERENCE_IN_DATA:
            ;
        }
    }
}

static void R_X86_64_32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced){
    printf("row = %d,col = %d,symbol referenced = %s\n",row_referencing,col_referencing,sym_referenced->st_name);
}

static void R_X86_64_PC32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced){
    printf("row = %d,col = %d,symbol referenced = %s\n",row_referencing,col_referencing,sym_referenced->st_name);
}

static void R_X86_64_PLT32_handler(elf_t* dst,sh_entry_t* sh,int row_referencing,int col_referencing,int addend, st_entry_t* sym_referenced){
    printf("row = %d,col = %d,symbol referenced = %s\n",row_referencing,col_referencing,sym_referenced->st_name);
}