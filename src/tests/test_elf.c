#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "headers/common.h"
#include "headers/linker.h"


extern int read_elf(const char* filename, uint64_t bufassr);
extern void parse_elf(char* filename, elf_t* elf);
extern void free_elf(elf_t* elf); 


// test func read_elf
/*
int main(){
    char buf[MAX_ELF_FILE_LENGTH][MAX_ELF_FILE_WIDTH];
    int count = read_elf("./files/exe/sum.elf.txt",(uint64_t)&buf);
    for(int i=0;i<count;++i){
        printf("%s\n",buf[i]);
    }
    return 0;
}
*/

//test func parse_elf
int main(){
    elf_t elf;
    parse_elf("./files/exe/sum.elf.txt",&elf);
    free_elf(&elf);
    return 0;
}