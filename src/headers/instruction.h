#ifndef INSTRUCTION_GUARD
#define INSTRUCTION_GUARD

#include <stdint.h>

/*====================================================*/
/*           instruction set architecture             */
/*====================================================*/

// instruction operator type
typedef enum{
    INST_MOV,               //0
    INST_PUSH,              //1
    INST_POP,               //2
    INST_LEAVE,             //3
    INST_CALL,              //4
    INST_RET,               //5
    INST_ADD,               //6
    INST_SUB,               //7
    INST_CMP,               //8
    INST_JNE,               //9
    INST_JMP,               //10
}op_t;

// operand type
typedef enum{
    EMPTY,                           //0 => " "
    IMM,                             //1 => "$0x1234"
    REG,                             //2 => "%rax"
    MEM_IMM,                         //3 => "0xabcd"
    MEM_REG1,                        //4 => "(%rsp)"
    MEM_IMM_REG1,                    //5 => "0xabcd(%rsp)"
    MEM_REG1_REG2,                   //6 => "(%rsp,%rbx)"
    MEM_IMM_REG1_REG2,               //7 => "0xabcd(%rsp,%rbx)"
    MEM_REG2_SCAL,                   //8 => "(,%rbx,8)"
    MEM_IMM_REG2_SCAL,               //9 => "0xabcd(,%rbx,8)"
    MEM_REG1_REG2_SCAL,              //10 => "(%rsp,%rbx,8)"
    MEM_IMM_REG1_REG2_SCAL,          //11 => "0xabcd(%rsp,%rbx,8)"
}od_type_t;

// operand struct
typedef struct{
    od_type_t type;    // IMM, REG, MEM
    uint64_t  imm;     // immediate number
    uint64_t  scal;    // scale number of register 2
    uint64_t  reg1;    // main register
    uint64_t  reg2;    //register 2
}od_t;

// local vairables are allocated in stack in run-time
// don't consider local STATIC variable
// ref: Computer System: A Programmer's Perspective 3td
// Chapter 7 Linking: 7.5 Symbol and Symbol Tables
typedef struct{
    op_t    op;      // enmu of operator, e.g. mov, call, etc.
    od_t    src;     // operand src of instruction     
    od_t    dst;     // operand dst of instruction
}inst_t;

#endif