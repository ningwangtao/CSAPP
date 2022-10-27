#!/usr/bin/python3
# usage:
#   ./cmd.py argv[1] argv[2], ...
#   /usr/bin/python3 ./cmd.py argv[1] argv[2], ...

import sys
import os
import subprocess
from pathlib import Path

# print arguments
i = 0
for argv in sys.argv:
    print("[",i,"]",argv)
    i += 1

print("------------------------")

KEY_MACHINE = "m"
KEY_LINKER = "l"

EXE_BIN_MACHINE = "./bin/test_machine"
EXE_BIN_LINKER = "./bin/test_elf"

def make_build_directory():
    if not os.path.isdir("./bin/"):
        os.mkdir("./bin/")

def format_include(s):
    a = "#include<headers/"
    b = "#include<"

    # check include
    if s.startswith(a):
        s = "#include \"headers/" + s[len(s):]
        for j in range(len(s)):
            if s[j] == '>':
                l = list(s)
                l[j] = "\""
                s = "".join(l)
    elif s.startswith(b):
        s = "#include <" + s[len(b):]
    return s

def format_whiteline(s):
    space = 0
    for c in s:
        if c == ' ':
            space += 1
    if space == len(s) - 1 and s[-1] == '\n':
        s = "\n"
    return s

def format_code():
    # get line with path
    filelist = list(Path(".").rglob("*.[ch]"))
    # recursively add lines to every .c and file
    print("recursively check every .c and file")
    for filename in filelist:
        try:
            with open(filename,"r",encoding='ascii') as fr:
                content = fr.readlines()
                for i in range(len(content)):
                    content[i] = format_include(content[i])
                    content[i] = format_whiteline(content[i])
                fr.close()
                # reopen and write data: this is a safer approach
                # tey to not open in r+ mode
                with open(filename,"w",encoding='ascii') as fw:
                    fw.writelines(content)
                    fw.close()
        except UnicodeDecodeError:
            print(filename)

def count_lines():
    # get files with paths
    filelist = list(Path(".").rglob("*.[ch]"))
    name_count = []
    total_count = 0 
    maxfilename = 0
    for filename in filelist:
        count = 0
        for index,line in enumerate(open(filename,'r')):
            count += 1
        name_count += [[str(filename),count]]
        total_count += count
        if len(str(filename)) > maxfilename:
            maxfilename = len(str(filename))
    # print result
    print("count .c and .h files lines:")
    sortedlist = sorted(name_count,key = lambda x:x[1],reverse=True)
    for [filename,count] in sortedlist:
        print(filename, end="")
        n = (int(maxfilename/4) + 1) * 4
        for i in range(n - len(filename)):
            print(" ",end="")
        print(count)
    print("\nTotal:",total_count)

def build(key):
    make_build_directory()
    gcc_map = {
        KEY_MACHINE: [
                [
                    "/usr/bin/gcc-9",
                    "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
                    "-I","./src",
                    "./src/tests/test_machine.c",
                    "./src/common/print.c",
                    "./src/common/convert.c",
                    "./src/common/cleanup.c",
                    "./src/algorithm/trie.c",
                    "./src/algorithm/array.c",
                    "./src/hardware/cpu/isa.c",
                    "./src/hardware/cpu/mmu.c",
                    "./src/hardware/memory/dram.c",
                    "-o",EXE_BIN_MACHINE
                ]
            ],
        KEY_LINKER:[
                [
                    "/usr/bin/gcc-9",
                    "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
                    "-I","./src",
                    "./src/tests/test_elf.c",
                    "./src/common/print.c",
                    "./src/common/convert.c",
                    "./src/common/cleanup.c",
                    "./src/common/tagmalloc.c",
                    "./src/algorithm/hashtable.c",
                    "./src/algorithm/array.c",
                    "./src/algorithm/linkedlist.c",
                    "./src/linker/parseElf.c",
                    "./src/linker/staticlink.c",
                    "-o",EXE_BIN_LINKER
                ],
                [
                    "/usr/bin/gcc-9",
                    "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
                    "-I","./src",
                    "-shared","-fPIC",
                    "./src/common/print.c",
                    "./src/common/convert.c",
                    "./src/common/cleanup.c",
                    "./src/common/tagmalloc.c",
                    "./src/algorithm/hashtable.c",
                    "./src/algorithm/array.c",
                    "./src/algorithm/linkedlist.c",
                    "./src/linker/parseElf.c",
                    "./src/linker/staticlink.c",
                    "-o","./bin/staticlinker.so"
                ],
                [
                    "/usr/bin/gcc-9",
                    "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
                    "-I","./src",
                    "./src/common/print.c",
                    "./src/common/convert.c",
                    "./src/common/cleanup.c",
                    "./src/common/tagmalloc.c",
                    "./src/algorithm/hashtable.c",
                    "./src/algorithm/array.c",
                    "./src/algorithm/linkedlist.c",
                    "./src/linker/linker.c",
                    "-ldl","-o","./bin/link"
                ]
            ]
    }

    if not key in gcc_map:
        print("input the correct build key:",gcc_map.keys())
        exit()
    for command in gcc_map[key]:
        subprocess.run(command)

def run(key):
    assert(os.path.isdir("./bin/"))
    bin_map = {
        KEY_MACHINE:[EXE_BIN_MACHINE],
        KEY_LINKER:[EXE_BIN_LINKER],
        "dll":["./bin/link","main","sum","-o","output"],
    }
    if not key in bin_map:
        print("input the correct binary key:",bin_map.keys())
        exit()
    subprocess.run(bin_map[key])

def debug(key):
    assert(os.path.isdir("./bin/"))
    bin_map = {
        KEY_MACHINE:EXE_BIN_MACHINE,
        KEY_LINKER:EXE_BIN_LINKER
    }
    if not key in bin_map:
        print("input the correct binary key:",bin_map.keys())
        exit()
    subprocess.run(["/usr/bin/gdb",bin_map[key]])

def mem_check(key):
    assert(os.path.isdir("./bin/"))
    bin_map = {
        KEY_MACHINE:EXE_BIN_MACHINE,
        KEY_LINKER:EXE_BIN_LINKER
    }
    if not key in bin_map:
        print("input the correct binary key:",bin_map.keys())
        exit()
    subprocess.run([
        "/usr/bin/valgrind",
        "--tool=memcheck",
        "--leak-check=full",
        bin_map[key]
    ])

def clean():
    # rm -f *.o *~ $(BIN_HARDWARE) EXE_BIN_LINKER
    subprocess.run([
        "rm",
        "-f",
        "*.o",
        "*~",
        EXE_BIN_MACHINE,
        EXE_BIN_LINKER,
        "./bin/link",
        "./bin/staticlinker.so",
        "./files/exe/output.eof.txt"
    ])

# main
assert(len(sys.argv) >= 2)

# single argument "python3 cmd.py argv[1]"
argv_1_lower = sys.argv[1].lower()
if "build".startswith(argv_1_lower):
    assert(len(sys.argv) == 3)
    build(sys.argv[2])
elif "run".startswith(argv_1_lower):
    assert(len(sys.argv) == 3)
    run(sys.argv[2])
elif "debug".startswith(argv_1_lower):
    assert(len(sys.argv) == 3)
    debug(sys.argv[2])
elif KEY_MACHINE.lower().startswith(argv_1_lower):
    build(KEY_MACHINE)
    run(KEY_MACHINE)
elif KEY_LINKER.lower().startswith(argv_1_lower):
    build(KEY_LINKER)
    run(KEY_LINKER)
elif "memorycheck".startswith(argv_1_lower):
    assert(len(sys.argv) == 3)
    mem_check(sys.argv[2])
elif "clean".startswith(argv_1_lower):
    clean()
elif "count".startswith(argv_1_lower):
    count_lines()
elif "format".startswith(argv_1_lower):
    format_code()
