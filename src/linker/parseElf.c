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
    (*ent) = malloc(count_col * sizeof(char*));
    uint8_t col_index = 0;
    uint8_t col_width = 0;
    char col_buf[32];
    for(int i=0;i<len+1;++i){
        if(str[i] == ',' || str[i] == '\0'){
            assert(col_index < count_col);

            // malloc and copy
            char* col = malloc((col_width + 1) * sizeof(char));
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
 * @param ent 
 * @param n 
 */
static void free_table_entry(char** ent, int n){
    for(int i=0;i<n;++i){
        free(ent[i]);
        ent[i] = NULL;
    }
    free(ent);
    ent = NULL;
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

    free_table_entry(cols,num_cols);
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
    if(strcmp(cols[1],"STB_LOCAL") == 0){
        ste->bind = STB_LOCAL;
    }else if(strcmp(cols[1],"STB_GLOBAL") == 0){
        ste->bind = STB_GLOBAL;
    }else if(strcmp(cols[1],"STB_WEAK") == 0){
        ste->bind = STB_WEAK;
    }else{
        printf("symbol bind is neiter LOCAL,GLOBAL,nor WEAK\n");
        exit(1);
    }

    // select symbol table type
    if(strcmp(cols[2],"STT_NOTYPE") == 0){
        ste->type = STT_NOTYPE;
    }else if(strcmp(cols[2],"STT_OBJECT") == 0){
        ste->type = STT_OBJECT;
    }else if(strcmp(cols[2],"STT_FUNC") == 0){
        ste->type = STT_FUNC;
    }else{
        printf("symbol bind is neiter NOTYPE,OBJECT,nor FUNC\n");
        exit(1);
    }

    strcpy(ste->st_shndx,cols[3]);
    ste->st_value = string2uint(cols[4]);
    ste->st_size = string2uint(cols[5]);

    free_table_entry(cols,num_cols);
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
 * @param filename 
 * @param elf 
 */
void parse_elf(char* filename, elf_t* elf){
    assert(elf != NULL);
    int line_count = read_elf(filename,(uint64_t)(&(elf->buffer)));
    for(int i=0;i<line_count;++i){
        printf("[%d]\t%s\n",i,elf->buffer[i]);
    }

    // parse section header
    elf->sht_count = (uint8_t)string2uint(elf->buffer[1]);
    elf->sht = malloc(elf->sht_count * sizeof(sh_entry_t));
    sh_entry_t* symt_sh = NULL;
    for(int i=0;i<elf->sht_count;++i){
        parse_sh(elf->buffer[2 + i],&(elf->sht[i]));
        print_sh_entry(&(elf->sht[i]));
        // get symbol table information from section header entry
        if(strcmp(elf->sht[i].sh_name,".symtab") == 0){
            symt_sh = &(elf->sht[i]);
        }
    }

    // parse symbol table
    assert(symt_sh != NULL);
    elf->symt_count = symt_sh->sh_size;
    elf->symt = malloc(elf->symt_count * (sizeof(st_entry_t)));
    for(int i=0;i<elf->symt_count;++i){
        parse_symtab(elf->buffer[symt_sh->sh_offset + i],&(elf->symt[i]));
        print_symtab_entry(&(elf->symt[i]));
    }
}

/**
 * @brief 
 * 
 * @param elf 
 */
void free_elf(elf_t* elf){
    assert(elf != NULL);
    free(elf->sht);
    elf->sht = NULL;
    free(elf->symt);
    elf->symt = NULL;
}
