#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "headers/linker.h"
#include "headers/common.h"


/**
 * @brief parse one line (entey) from char* str
 * 
 * @param str 
 * @param ent 
 * @return int 
 */
static int parse_table_entry(char* str, char*** ent){
    // parse line as table entries
    uint8_t count_col = 1;
    uint8_t len = strlen(str);

    // count columns
    for(int i=0;i<len;++i){
        if(str[i] == ','){
            count_col++;
        }
    }

    // malloc and create list
    char** arr = tag_malloc(count_col * sizeof(char*),"parse_table_entry");
    (*ent) = arr;
    uint8_t col_index = 0;
    uint8_t col_width = 0;
    char col_buf[32];
    for(int i=0;i<len+1;++i){
        if(str[i] == ',' || str[i] == '\0'){
            assert(col_index < count_col);

            // malloc and copy
            char* col = tag_malloc((col_width + 1) * sizeof(char),"parse_table_entry");
            for(int j=0;j<col_width;++j){
                col[j] = col_buf[j];
            }
            col[col_width] = '\0';

            // update
            (*ent)[col_index] = col;
            col_index++;
            col_width = 0;
        }else{
            assert(col_width < 32);
            col_buf[col_width] = str[i];
            col_width++;
        }
    }
    return count_col;
}

/**
 * @brief 
 * 
 * @param str 
 * @param sh 
 */
static void parse_sh(char* str, sh_entry_t* sh){
    // .text,0x0,4,22
    char** cols;
    int num_cols = parse_table_entry(str,&cols);
    assert(num_cols == 4);

    strcpy(sh->sh_name,cols[0]);
    sh->sh_addr = string2uint(cols[1]);
    sh->sh_offset = string2uint(cols[2]);
    sh->sh_size = string2uint(cols[3]);
}

/**
 * @brief 
 * 
 * @param sh 
 */
static void print_sh_entry(sh_entry_t* sh){
    debug_printf(DEBUG_LINKER,"%s\t%x\t%d\t%d\n",sh->sh_name,sh->sh_addr,sh->sh_offset,sh->sh_size);
}

/**
 * @brief 
 * 
 * @param str 
 * @param ste 
 */
static void parse_symtab(char* str,st_entry_t* ste){
    // sum,STB_GLOBAL,STT_FUNC,.text,0,22
    char** cols;
    int num_cols = parse_table_entry(str,&cols);
    assert(num_cols == 6);
    assert(ste != NULL);

    strcpy(ste->st_name,cols[0]);

    // elsect symbol table bind
    uint64_t bind_value;

    if(hashtable_get(link_constant_dict,cols[1],&bind_value) == 0){
        // failed
        printf("symbol bind is neither LOCAL,GLOBAL,nor WEAK\n");
        exit(0);
    }
    ste->bind = (st_bind_t)bind_value;

    // select symbol table type
    uint64_t type_value;
    if(hashtable_get(link_constant_dict,cols[2],&type_value) == 0){
        // failed
        printf("symbol bind is neiter NOTYPE,OBJECT,nor FUNC\n");
        exit(1);
    }
    ste->type = (st_type_t)type_value;

    strcpy(ste->st_shndx,cols[3]);
    ste->st_value = string2uint(cols[4]);
    ste->st_size = string2uint(cols[5]);
}

/**
 * @brief 
 * 
 * @param ste 
 */
static void print_symtab_entry(st_entry_t* ste){
    debug_printf(DEBUG_LINKER,"%s\t%d\t%d\t%s\t%d\t%d\n",
                ste->st_name,ste->bind,ste->type,ste->st_shndx,
                ste->st_value,ste->st_size
                );
}

/**
 * @brief 
 * 
 * @param str 
 * @param rte 
 */
static void parse_relocation(char* str, rel_entry_t* rte){
    // 4,7,R_X86_64_PC32,0,-4
    char** cols;
    int num_cols = parse_table_entry(str,&cols);
    assert(num_cols == 5);

    assert(rte != NULL);
    rte->r_row = string2uint(cols[0]);
    rte->r_col = string2uint(cols[1]);

    // select relocation type
    uint64_t type_value;
    if(hashtable_get(link_constant_dict,cols[2],&type_value) == 0){
        // failed
        printf("relocation type is neiter R_X86_64_32,R_X86_64_PC32,nor R_X86_64_PLT32\n");
        exit(0);
    }
    rte->type = (st_type_t)type_value;

    rte->sym = string2uint(cols[3]);
    uint64_t bitmap = string2uint(cols[4]);
    rte->r_addrend = *(int64_t *)&bitmap;
}

/**
 * @brief 
 * 
 * @param rte 
 */
static void print_relocation_entry(rel_entry_t* rte){
    debug_printf(DEBUG_LINKER,"%d\t%d\t%d\t%d\t%d\n",
        rte->r_row,
        rte->r_col,
        rte->type,
        rte->sym,
        rte->r_addrend);
}

/**
 * @brief 
 * 
 * @param filename 
 * @param bufaddr 
 * @return int 
 */
static int read_elf(const char* filename, uint64_t bufaddr){
    // open file and read
    FILE* fp;
    fp = fopen(filename,"r");
    if(fp == NULL){
        debug_printf(DEBUG_LINKER,"unable to open file %s\n",filename);
        exit(1);
    }

    // read text file line by line
    char line[MAX_ELF_FILE_WIDTH];
    uint8_t line_counter = 0;

    while(fgets(line,MAX_ELF_FILE_WIDTH,fp) != NULL){
        uint8_t len = strlen(line);
        if((len == 0) ||
            (len >= 1 && (line[0] == '\n' || line[0] == '\r' || line[0] == '\t')) ||
            (len >= 2 && (line[0] == '/' && line[1] == '/'))
        ){
            continue;
        }

        // check if is empty or white line
        uint8_t iswhite = 1;
        for (int i=0;i<len;++i){
            iswhite = iswhite && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r');
        }
        if(iswhite){
            continue;
        }

        // to this line, this line is not white and contains information
        if(line_counter < MAX_ELF_FILE_LENGTH){
            // store this line to buffer[line_counter]
            uint64_t addr = bufaddr + line_counter * MAX_ELF_FILE_WIDTH * sizeof(char);
            char* linebuf = (char*)addr;
            int i = 0;
            while(i<len && i<MAX_ELF_FILE_WIDTH){
                if(line[i] == '\n' || line[i] == '\r' || ((i+1<len) && (i+1<MAX_ELF_FILE_WIDTH) && line[i] == '/' &&line[i+1] == '/')){
                    break;
                }
                linebuf[i] = line[i];
                i++;
            }
            linebuf[i] = '\0';
            line_counter ++;
        }else{
            debug_printf(DEBUG_LINKER,"elf file %s is too long (> %d)\n",filename,MAX_ELF_FILE_WIDTH);
            fclose(fp);
            exit(1);
        }
    }
    fclose(fp);
    assert(string2uint((char*)bufaddr) == line_counter);
    return line_counter;
}

/**
 * @brief 
 * 
 */
static void init_dictionary(){
    if(link_constant_dict != NULL){
        return ;
    }

    link_constant_dict = hashtable_construct(4);

    hashtable_insert(&link_constant_dict,"R_X86_64_32",R_X86_64_32);
    hashtable_insert(&link_constant_dict,"R_X86_64_PC32",R_X86_64_PC32);
    hashtable_insert(&link_constant_dict,"R_X86_64_PLT32",R_X86_64_PLT32);

    hashtable_insert(&link_constant_dict,"STT_NOTYPE",STT_NOTYPE);
    hashtable_insert(&link_constant_dict,"STT_OBJECT",STT_OBJECT);
    hashtable_insert(&link_constant_dict,"STT_FUNC",STT_FUNC);

    hashtable_insert(&link_constant_dict,"STB_LOCAL",STB_LOCAL);
    hashtable_insert(&link_constant_dict,"STB_GLOBAL",STB_GLOBAL);
    hashtable_insert(&link_constant_dict,"STB_WEAK",STB_WEAK);

    print_hashtable(link_constant_dict);
}

/**
 * @brief 
 * 
 * @param filename 
 * @param elf 
 */
void parse_elf(char* filename, elf_t* elf){
    assert(elf != NULL);
    int line_count = read_elf(filename,(uint64_t)(&(elf->buffer)));
    for(int i=0;i<line_count;++i){
        printf("[%d]\t%s\n",i,elf->buffer[i]);
    }

    init_dictionary();

    // parse section header
    elf->sht_count = (uint8_t)string2uint(elf->buffer[1]);
    elf->sht = tag_malloc(elf->sht_count * sizeof(sh_entry_t),"parse_elf");
    sh_entry_t* symt_sh = NULL;
    sh_entry_t* rtext_sh = NULL;
    sh_entry_t* rdata_sh = NULL;
    for(int i=0;i<elf->sht_count;++i){
        parse_sh(elf->buffer[2 + i],&(elf->sht[i]));
        print_sh_entry(&(elf->sht[i]));
        // get symbol table information from section header entry
        if(strcmp(elf->sht[i].sh_name,".symtab") == 0){
            symt_sh = &(elf->sht[i]);
        }else if(strcmp(elf->sht[i].sh_name,".rel.text") == 0){
            rtext_sh = &(elf->sht[i]);
        }else if(strcmp(elf->sht[i].sh_name,".rel.data") == 0){
            rdata_sh = &(elf->sht[i]);
        }
    }

    // parse symbol table
    assert(symt_sh != NULL);
    elf->symt_count = symt_sh->sh_size;
    elf->symt = tag_malloc(elf->symt_count * (sizeof(st_entry_t)),"parse_elf");
    for(int i=0;i<elf->symt_count;++i){
        parse_symtab(elf->buffer[symt_sh->sh_offset + i],&(elf->symt[i]));
        print_symtab_entry(&(elf->symt[i]));
    }

    // parse relocation table .rel.text
    if(rtext_sh != NULL){
        elf->reltext_count = rtext_sh->sh_size;
        elf->reltext = tag_malloc(elf->reltext_count * sizeof(rel_entry_t),"parse_elf");
        for(int i=0; i<elf->reltext_count; ++i){
            parse_relocation(elf->buffer[rtext_sh->sh_offset + i],&(elf->reltext[i]));
            int st = elf->reltext[i].sym;
            assert(0<=st && st < elf->symt_count);
            print_relocation_entry(&(elf->reltext[i]));
        }
    }else{
        elf->reltext_count = 0;
        elf->reltext = NULL;
    }

    // parse relocation table .rel.data
    if(rdata_sh != NULL){
        elf->reldata_count = rdata_sh->sh_size;
        elf->reldata = tag_malloc(elf->reldata_count * sizeof(rel_entry_t),"parse_elf");
        for(int i=0; i<elf->reldata_count; ++i){
            parse_relocation(elf->buffer[rdata_sh->sh_offset + i],&(elf->reldata[i]));
            int st = elf->reldata[i].sym;
            assert(0<=st && st < elf->symt_count);
            print_relocation_entry(&(elf->reldata[i]));
        }
    }else{
        elf->reldata_count = 0;
        elf->reldata = NULL;
    }

    tag_sweep("parse_table_entry");
}

/**
 * @brief 
 * 
 * @param filename 
 * @param eof 
 */
void write_eof(const char* filename,elf_t* eof){
    // open destation elf file
    FILE* fp;
    fp = fopen(filename,"w");
    if (fp == NULL){
        debug_printf(DEBUG_LINKER,"unable to open file: %s\n",filename);
        exit(1);
    }

    for(int i=0; i< eof->line_count; i++){
        fprintf(fp,"%s\n",eof->buffer[i]);
    }
    fclose(fp);

    // free hash table
    hashtable_free(link_constant_dict);
}

/**
 * @brief 
 * 
 * @param elf 
 */
void free_elf(elf_t* elf){
    assert(elf != NULL);
    tag_free(elf->sht);
    tag_free(elf->symt);
    tag_free(elf->reltext);
    tag_free(elf->reldata);
//    tag_free(elf);
}
