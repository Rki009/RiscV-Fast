# Test program for RiscvSim

OPT_32		= -march=rv32i -mabi=ilp32
OPT_32c		= -march=rv32imafc -mabi=ilp32f
LDOPT_32	= -Wl,-Ttext=0x00000000 -Wl,-Tdata=0x40000000

OPT_64		= -march=rv64id -mabi=lp64d -mtune=rocket -mcmodel=medlow
LDOPT_64	= -Wl,-Ttext=0x00000000 -Wl,-Tdata=0x40000000

SRC = Test.cpp newlib.cpp dhry.c trap.c

# all: Test32.elf Test32c.elf Test64.elf Makefile
all: Test32.elf Makefile

Test32.elf: $(SRC) Makefile
	# rv32im - 32 bit build, 32 bit insn
	riscv64-unknown-elf-gcc $(LDOPT_32) $(OPT_32) -O2 -lm $(SRC) -o $@
	riscv64-unknown-elf-objdump -d -t -h $@ >Test32.lst
	riscv64-unknown-elf-objdump -s -j .rodata $@ >Test32_dat.lst

Test32c.elf: $(SRC) Makefile
	# rv32imafc - 32 bit build, 32 bit insn (compressed)
	riscv64-unknown-elf-gcc $(LDOPT_32) $(OPT_32c) -O2 $(SRC) -o $@
	riscv64-unknown-elf-objdump -d -t -h $@ >Test32c.lst

Test64.elf: $(SRC) Makefile
	# rv64id - 64 bit build, 32 bit insn
	riscv64-unknown-elf-gcc $(LDOPT_64) $(OPT_64) -O2 $(SRC) -o $@
	riscv64-unknown-elf-objdump -d -t -h $@ >Test64.lst
	
clean:
	rm -f *.elf *.lst *.o

clobber: clean

