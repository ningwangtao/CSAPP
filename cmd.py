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
            "/usr/bin/gcc-9",
            "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
            "-I","./src",
            "./src/tests/test_machine.c",
            "./src/common/printf.c",
            "./src/common/convert.c",
            "./src/common/trie.c",
            "./src/common/cleanup.c",
            "./src/hardware/cpu/isa.c",
            "./src/hardware/cpu/mmu.c",
            "./src/hardware/memory/dram.c",
            "-o",EXE_BIN_MACHINE],
        KEY_LINKER:[
            "/usr/bin/gcc-9",
            "-Wall","-g","-O0","-Werror","-std=gnu99","-Wno-unused-function",
            "-I","./src",
            "./src/tests/test_elf.c",
            "./src/common/printf.c",
            "./src/common/convert.c",
            "./src/linker/parseElf.c",
            "./src/linker/staticlink.c",
            "-o",EXE_BIN_LINKER]
    }

    if not key in gcc_map:
        print("input the correct build key:",gcc_map.keys())
        exit()
    subprocess.run(gcc_map[key])

def run(key):
    assert(os.path.isdir("./bin/"))
    bin_map = {
        KEY_MACHINE:EXE_BIN_MACHINE,
        KEY_LINKER:EXE_BIN_LINKER
    }
    if not key in bin_map:
        print("input the correct binary key:",bin_map.keys())
        exit()
    subprocess.run([bin_map[key]])

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
        EXE_BIN_LINKER
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
