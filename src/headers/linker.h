// include guards to prevent double declaration of any identifiers 
 // such as types, enums and static variables

 #ifndef LINKER_GUARD
 #define LINKER_GUARD

#include <stdint.h>
#include <stdio.h>
#include "headers/algorithm.h"

#define MAX_CHAR_SECTION_NAME (32)
// setion header entry
typedef struct{
    char sh_name[MAX_CHAR_SECTION_NAME];
    uint64_t sh_addr;
    uint64_t sh_offset; // line offset or effective line index
    uint64_t sh_size;
}sh_entry_t;


#define MAX_CHAR_SYMBOL_NAME (64)
// symbol table bind
typedef enum{
    STB_LOCAL,
    STB_GLOBAL,
    STB_WEAK
}st_bind_t;

// symbol table type
typedef enum{
    STT_NOTYPE,
    STT_OBJECT,
    STT_FUNC
}st_type_t;


// symbol table entry
typedef struct{
    char st_name[MAX_CHAR_SYMBOL_NAME];
    st_bind_t bind;
    st_type_t type;
    char st_shndx[MAX_CHAR_SYMBOL_NAME];     // section header index: e.g. .bss .text .data etc.
    uint64_t st_value;                       // in-section offset
    uint64_t st_size;                        // count of lines of symbol
}st_entry_t;

/*===================================*/
/*       relocation information      */
/*===================================*/

typedef enum{
    R_X86_64_32,
    R_X86_64_PC32,
    R_X86_64_PLT32,
}reltype_t;

hashtable_t* link_constant_dict;

// relocation entry type
typedef struct{
    /* this is what's different in our implamentation. instead of byte offset,
        we use line index+char offset to locate the symbol */
    uint8_t    r_row;       // line index of the symbol in buffer section
                            // for .rel.text, that's the line index in .text section
                            // for .rel.data, thst's the line index in .data section
    uint8_t    r_col;       // char offset in the buffer line
    reltype_t  type;        // relocation type
    uint8_t    sym;         // symbol table index
    int    r_addrend;   // constant part of relocation expression
}rel_entry_t;


#define MAX_ELF_FILE_LENGTH  (64)   // max 64 effective lines -> row
#define MAX_ELF_FILE_WIDTH   (128)  // max 128 chars per line -> columns

typedef struct{
    char buffer[MAX_ELF_FILE_LENGTH][MAX_ELF_FILE_WIDTH];

    uint8_t         sht_count;    // section header number
    sh_entry_t*     sht;          // section header pointer

    uint8_t         symt_count;   // symbol table number
    st_entry_t*     symt;         // symbol table pointer

    uint8_t         line_count;

    uint8_t         reltext_count;
    rel_entry_t*    reltext;

    uint8_t         reldata_count;
    rel_entry_t*    reldata;
}elf_t;

void parse_elf(char* filename, elf_t* elf);
void free_elf(elf_t* elf);
void link_elf(elf_t** src, int num_src, elf_t* dst);
void write_eof(const char* filename,elf_t* eof);

 #endif
