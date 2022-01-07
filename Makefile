# CC = "C:\Program Files (x86)\Embarcadero\BCC102\bin\bcc32x.exe"
# CFLAGS = -Os -fwritable-strings

PROJ=RiscVFast

ISA_DIR=../RiscV-ISA

# OPT = -DELF64=1 -DRV64=1
OPT = -DELF32=1 -DRV32=1

# CFLAGS = -O2 -Wno-write-strings $(OPT)
CFLAGS = -g -O3 -Wall -Wno-write-strings $(OPT)

# System name: "linux"
UNAME_S := $(shell uname -s)
# Processor: "x86_64"
UNAME_P := $(shell uname -p)
ifeq ($(UNAME_P),x86_64)
	CFLAGS += -m64 -mrdrnd
endif


LDLAGS = -g

INCL = -I.\Include
CC = g++

# SRC = LogIt.cpp Functions.cpp mystrftime.cpp Epoch.cpp Erlang.cpp
# SRC = $(wildcard *.cpp)
SRC = $(PROJ).cpp Elf64.cpp Cpu.cpp Decode.cpp Compressed.cpp Util.cpp # getopt.c
OBJ = $(SRC:%.cpp=%.o)
HDR = $(wildcard *.h)

# .SILENT: clean

.PHONY: all run clean clobber update run_sw++

%.o:%.cpp $(HDR) Makefile
	$(CC) $(INCL) -MD $(CFLAGS) $(CPPFLAGS) -c $< -o $@

all: $(PROJ)
#	@echo "Src = $(SRC)"
#	@echo "Obj = $(OBJ)"
#	@echo "Hdr = $(HDR)"
#	@echo "Proc = $(UNAME_P)"

$(PROJ): $(OBJ) $(HDR) Makefile
	$(CC) $(LDLAGS) $(OBJ) -o $@
	
run: all
ifeq ($(UNAME_P),x86_64)
	cd ./sw/Test_001 && make all && cd ..
endif
	./$(PROJ) --newlib ./sw/Test_001/Test32.elf

run_sw++: all
ifeq ($(UNAME_P),x86_64)
	cd ./sw/sw++ && make all && cd ..
endif
	./$(PROJ) --core ./sw/sw++/bin/progmem.elf

# gdb - run gdb
#	commands:
#		r 		- run
#		bt 		- backtrack (show call stack)
#		p name	- print variable
#		q		- quit	
gdb: all
	gdb -ex=run --arg ./$(PROJ) --core -l ./sw/sw++/bin/progmem.elf
	
update: clean
	cp $(ISA_DIR)/OpTable.h .
	cp $(ISA_DIR)/OpTable_rv64.cpp .
	cp $(ISA_DIR)/OpTable_rv32.cpp .
	cp $(ISA_DIR)/VliwTable_rv32.h .
	cp $(ISA_DIR)/VliwTable_rv32.cpp .
	cp $(ISA_DIR)/VliwTable_rv64.h .
	cp $(ISA_DIR)/VliwTable_rv64.cpp .

clean:
	rm -f *.o *.d *.log

clobber: clean
	rm -f ./$(PROJ)
	
#-include *.d
#-include $(wildcard bin/*.d)
-include $(wildcard *.d)

