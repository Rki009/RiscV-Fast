// riscv.cpp - RiscV Simulator
//
//	(C) Copyright, Ron K. Irvine, 2005. All rights reserved.
//
//	Performance:
//		On a 3.06 GHz Pentium 4, the processor benchmarks at 20 DMIPS
//

// #include <vcl.h>
// #pragma hdrstop

// #include "C:\Ron\Src\MyLib\CppLib.hpp"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "getopt.h"

#include "RiscVFast.h"
#include "Cpu.h"
#include "Elf64.h"

int Version = 1;
int Release = 0;

// options
bool doSignature = false;
bool quiet = true;				// run quite -- very minimal output

FILE* lfp;		// log file
// const char* elfFile = "C:\\Ubuntu\\RISC-V\\RISC-Vsim\\Test\\Test64";
const char* elfFile = NULL;

// Execution Options
bool do_newlib = false;		// true = process newlib syscalls
bool do_core = false;		// true = allows write to instruction memory

// Create the processor
Memory* memory = new Memory;
Cpu* cpu = new Cpu;

// Fake devices and Host
uint32_t	hostAddr = HOST_BASE;
uint32_t	hostSize = HOST_SIZE;	// 64K

uint32_t 	heap_len = HEAP_SIZE;

void uartWrite(uint32_t value) {
	uint8_t c = value;
	// printf("Uart: '%c' (0x%02x)\n", isprint(c)?c:'.', c);
	printf("%c", c);
	// fprintf(stdout, "%c", c);
	fflush(stdout);
	static FILE* ofp;
	if(ofp == NULL) {
		ofp = fopen("stdout.txt", "w");
	}
	fputc(c, ofp);
	fflush(ofp);
	return;
};

// Host - Connection to a host machine ...
//	Minimal is just a single port for 'Uart' data
#define CMD_PRINT_CHAR 1
#define CMD_POWER_OFF  2
uint32_t host(bool rw, uint32_t addr, uint32_t value) {

	if(addr == 0xfffffffc) {
		uint16_t cmd = value>>16;
		// if (cmd == CMD_POWER_OFF || (value&0xff) == 0xff)) {
		if(cmd == CMD_POWER_OFF) {
			cpu->stop = true;
			return 0;
		}
		uartWrite(value);
		return 0;
	}

	if(addr == 0xffffff00 && rw) {
		// printf("Debug: %08x\n", value);
		return 0;
	}

	// some other type of host read/write
	printf("host: %d,%08x,%08x\n", rw, addr, value);

	return 0;
}

const char* buildDate(void) {
	static char buf[64];
	char day[4];
	char month[4];
	char year[8];
	char tod[8];
	sprintf(day, "%2.2s", &__DATE__[4]);
	if(day[0] == ' ') {
		sprintf(day, "%1.1s",  &__DATE__[5]);
	}
	sprintf(month, "%3.3s", &__DATE__[0]);
	sprintf(year, "%4.4s", &__DATE__[7]);
	sprintf(tod, "%5.5s", __TIME__);
	// sprintf(buf, "%s, %s-%s-%s", tod, day, month, year);
	sprintf(buf, "%s-%s-%s %s", day, month, year, tod);
	return buf;
}

uint32_t getValue(const char* argv) {
	const char* s = argv;
	if(*s == '=') {
		++s;
	}
	return strtol(s, NULL, 16);
}

void getAddrSize(const char* str, uint32_t* addr, uint32_t* size) {
	const char* s = str;
	if(*s == '=') {
		++s;
	}
	char* pEnd;
	*addr = strtol(s, &pEnd, 16);
	s = pEnd;
	if(*s == ':') {
		*size = strtol(++s, &pEnd, 16);
	}
}

const char* reg_list[32] {
	"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
	"s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
	"a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
	"s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void dump_reg(char* reg) {
	if(reg != NULL && strcmp(reg, "all")!=0) {
		int n;
		if(reg[0] == 'x') {
			n = atoi(&reg[1]);
			printf("%s: %08x\n", reg, cpu->reg[n]);
			return;
		}
		for(n = 0; n<32; ++n) {
			if(strcmp(reg, reg_list[n])==0) {
				printf("%s: %08x\n", reg, cpu->reg[n]);
				return;
			}
		}
		printf("Unknown reg: %s\n", reg);
		return;
	}

	for(int i=0; i<32; ++i) {
		printf("%4s: 0x%08x", reg_list[i], cpu->reg[i]);
		if((i%4) == 3) {
			printf("\n");
		}
	}
}

//	Interactive Debug Mode
//	======================
//		Interactive commands:
//		core [num]						# Set current core to [num], display current core if [num] is omitted
//		reg [reg]                		# Display [reg] (all if omitted)
//		freg <reg>               		# Display float <reg> as hex
//		fregh <reg>              		# Display half precision <reg>
//		fregs <reg>              		# Display single precision <reg>
//		fregd <reg>              		# Display double precision <reg>
//		vreg [reg]               		# Display vector [reg] (all if omitted) in <core>
//		pc [addr]						# Set current pc to [addr], display current pc if [addr] is omitted
//		mem <hex addr>                  # Show contents of physical memory
//		dump <hex addr>					# Dump 256 bytes of physical memory
//		str <hex addr>           		# Show NUL-terminated C string at <hex addr>
//		until reg <reg> <val>    		# Stop when <reg> equals <val>
//		until pc <val>           		# Stop when PC hits <val>
//		untiln pc <val>          		# Run noisy and stop when PC hits <val>
//		until mem <addr> <val>          # Stop when memory <addr> becomes <val>
//		while reg <reg> <val>    		# Run while <reg> is <val>
//		while pc <val>           		# Run while PC is <val>
//		while mem <addr> <val>          # Run while memory <addr> is <val>
//		run [count]                     # Resume noisy execution (until Ctrl+C, or [count] insns)
//		r [count]                       # Alias for run
//		rs [count]                      # Resume silent execution (until Ctrl+C, or [count] insns)
//		quit                            # End the simulation
//		q                               # Alias for quit
//		help                            # This screen!
//		h                               # Alias for help
//		Keys:							#
//			Enter						# run 1
//			Ctrl+C						# stop execution
//

void debug_help(void) {
	printf("Interactive Debug Mode\n");
	printf("======================\n");
	printf("core [num]					Set the current core to [num], print the current core number\n");
	printf("quit or q					End the simulation\n");
	printf("pc [addr]					Set the program counter, print the current pc\n");
	printf("mem addr [value]			Set memory address to [value], print the memory contents\n");
	printf("reg [name]					Show register [name], or show all\n");
	printf("run [num]					Run for [num] cycles, or forever is no [num]\n");
	printf("r [num]						Same as run\n");
	printf("step or s					Step, execute next instruction\n");
	printf("<Enter>						Enter key, same as step\n");
	printf("help or h					Show help\n");
	printf("  [option]					Option in above commands, if absent command just prints\n");
}

void show_current(void) {
	char buf[256];
	uint32_t pc = cpu->pc;
	uint32_t insn = memory->read32(pc);
	disasm(pc, insn, buf);
	printf("0x%08x: 0x%08x  %s\n", pc, insn, buf);
}

int debug_mode(void) {
	int core_id = 0;	// default to core 0

	char* args[16];	// allow up to 16 command parts
	int nargs = 0;
	show_current();
	for(;;) {
		char buf[256];

		printf(": "); fflush(stdout);
		fgets(buf, sizeof(buf)-1, stdin);
		// zap trailing '\n'
		for(char* s=buf; *s; ++s) {
			if(*s == '\n') {
				*s = '\0';
				break;
			}
		}

		// parse the args
		char* s = buf;
		for(nargs = 0; nargs<16; ++nargs) {
			while(isspace(*s)) {
				++s;
			}
			if(*s == '\n' || *s == '\0') {
				*s = '\0';
				break;
			}
			args[nargs] = s;
			while(!isspace(*s) && *s != '\0') {
				++s;
			}
			if(*s != '\0') {
				*s++ = '\0';
			}
		}
		// for(int i=0; i<nargs; ++i) printf("[%d] %s\n", i, args[i]);
		if(nargs == 0) {
			cpu->run(1);
			show_current();
		}
		else if(strcmp(args[0], "q")==0 || strcmp(args[0], "quit")==0) {
			return true;
		}
		else if(strcmp(buf, "h")==0 || strcmp(buf, "help")==0) {
			debug_help();
		}
		else if(strcmp(args[0], "core")==0) {
			if(nargs == 2) {
				core_id = strtol(args[1], NULL, 0);
			}
			printf("Core: %d\n", core_id);
		}
		else if(strcmp(buf, "r")==0 || strcmp(buf, "run")==0) {
			int n = -1;
			if(nargs == 2) {
				n = strtol(args[1], NULL, 0);
			}
			printf("Run: %d\n", n);
			cpu->run(n);
			show_current();
		}
		else if(strcmp(buf, "s")==0 || strcmp(buf, "step")==0) {
			cpu->run(1);
			show_current();
		}
		else if(strcmp(buf, "pc")==0) {
			if(nargs == 2) {
				cpu->pc = strtol(args[1], NULL, 0);
			}
			printf("PC: %08x\n", cpu->pc);
		}
		else if(strcmp(args[0], "mem")==0) {
			int32_t addr = 0;
			if(nargs < 2) {
				printf("No address given!\n");
			}
			addr = strtol(args[1], NULL, 0);
			if(nargs == 3) {
				uint32_t value = addr = strtol(args[2], NULL, 0);
				memory->write32(addr, value);
			}
			printf("0x%08x: 0x%08x\n", addr, memory->read32(addr));
		}
		else if(strcmp(buf, "reg")==0) {
			if(nargs == 2) {
				dump_reg(args[1]);
			}
			else {
				dump_reg(NULL);
			}
		}
		else {
			printf("Unknown command '%s'\n", args[0]);
		}

	}
}

const char* rn2[32] {
	"x0", "ra", "sp", "gp",
	"tp", "t0", "t1", "t2",
	"s0", "s1", "a0", "a1",
	"a2", "a3", "a4", "a5",

	"a6", "a7", "s2", "s3",
	"s4", "s5", "s6", "s7",
	"s8", "s9", "sa", "sb",	//sa = s10, sb = s11
	"t3", "t4", "t5", "t6"
};

void disasm_all(uint32_t start, uint32_t len) {
	const char* outfile = "disasm_out.txt";
	FILE* ofp = fopen(outfile, "w");
	printf("Disasm => %s\n", outfile);
	char buf[256];
	for(uint32_t i=start; i < (start+len); i += 4) {
		uint32_t insn = memory->read32(i);
		disasm(i, insn, buf);
		Vliw vliw;
		decode2vliw(i, insn, &vliw);
		fprintf(ofp, "%08x: %08x [%2d %s,%s,%s,%08x] %s\n", i, insn,
			vliw.opcode, rn2[vliw.rd], rn2[vliw.rs1], rn2[vliw.rs2], vliw.imm, buf);
	}
	fclose(ofp);
}

void usage(void) {
	printf("	-T addr:size    - .text base addresss\n");
	printf("	-D addr:size    - .data base addresss\n");
	printf("	-H addr:size	- host base addresss\n");
	printf("	-v			    - verbose\n");
	printf("	-q			    - quiet\n");
	printf("    -c              - compile to Warp output\n");
	printf("    -d              - output disassembled code\n");
	printf("    -t              - output execution trace\n");
	printf("	-i			    - interactive debug mode\n");
	printf("	-u			    - unified memory, .text/.data same address space\n");
	printf("	--newlib	    - support newlib semi-hosting\n");
	// printf("	--core	        - core I+D memory\n");
};

int main(int argc, char** argv) {
	bool vflag = false;			// verbose
	bool c_flag = false;		// c - compile to Warp output
	bool lflag = false;			// logfile
	bool uflag = false;			// unified memory, I&D same address space
	bool debug_flag = false;	// interactive debugging
	bool dis_flag = false;		// output disassemble
	bool trace_flag = false;	// output execution trace

	// int bflag = 0;
	// char* breakStr = NULL;
	char* textBase = NULL;
	char* dataBase = NULL;
	char* hostBase = NULL;
	int index;
	int c;

	// process any long options first
	for(index = 1; index < argc; index++) {
		const char* op = argv[index];
		int nremove = 1;		// default to remove 1 option
		if(op[0]=='-' && op[1] == '-') {
			if(strcmp(argv[index], "--newlib")==0) {
				do_newlib = true;
				if(!quiet) {
					printf("Newlib = %d\n", do_newlib);
				}
			}
			else if(strcmp(argv[index], "--core")==0) {
				do_core = true;
				if(!quiet) {
					printf("Do_core = %d\n", do_core);
				}
			}
			else {
				fprintf(stderr, "Unknown option: %s\n", argv[index]);
			}
			// remove the op
			for(int k=0; k<nremove; ++k) {
				for(int j=index; j<argc; ++j) {
					argv[j] = argv[j+1];
				}
				--argc;
			}
		}
	}

	opterr = 0;
	while((c = getopt(argc, argv, "cdilvqub:T:D:H:")) != -1) {
		// printf("c = '%c'\n", c);

		switch(c) {
		case '-':
			printf("Option: %s\n", optarg);
			vflag = 0;
			break;

		// quiet flag
		case 'q':
			quiet = !quiet;
			vflag = 0;
			break;

		// c - compile to Warp output
		case 'c':
			c_flag = true;
			break;

		// t - output execution trace
		case 't':
			trace_flag = true;
			break;

		// d - output disassemble
		case 'd':
			dis_flag = true;
			break;

		// interactive debug flag
		case 'i':
			debug_flag = !debug_flag;
			break;

		case 'v':
			vflag = true;
			break;

		// -u, unified memory
		case 'u':
			uflag = true;
			break;

		case 'l':
			lflag = true;
			break;

		// case 'b':
		//	breakStr = optarg;
		//	break;

		// -T addr, .text base address
		case 'T':
			textBase = optarg;
			break;

		// -D addr, .data base address
		case 'D':
			dataBase = optarg;
			break;

		// -H addr, host base address
		case 'H':
			hostBase = optarg;
			break;

		case '?':
			if(optopt == 'c') {
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			}
			else if(isprint(optopt)) {
				//fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				fprintf(stderr, "Unknown option: %s\n", argv[optind]);
			}
			else {
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
			}
			return 1;

		default:
			// abort();
			printf("default = '%c'\n", c);
		}
	}
	// printf("vflag = %d, breakStr = %s\n", vflag, breakStr);

	for(index = optind; index < argc; index++) {
		if(!quiet) {
			printf("Non-option argument %s\n", argv[index]);
		}
		if(index == optind) {
			elfFile = argv[index];
		}
	}

	if(!quiet) {
		printf("MySim - RV32/Rv64\n");
		printf("  (C) Ron K. Irvine, 2022.\n");
		// printf("  Build: %s %s\n", __DATE__, __TIME__);
		printf("  Build: V%d.%02d, %s\n", Version, Release, buildDate());
	}

	if(textBase != NULL) {
		uint32_t text_addr = 0;
		uint32_t text_size = 0;
		getAddrSize(textBase, &text_addr, &text_size);

		memory->insnMem->memBase = text_addr;
		if(text_size != 0) {
			memory->insnMem->memLen = text_size;
		}
		printf("Textbase = '%s' => 0x%08x:0x%08x\n", textBase, text_addr, text_size);
	}

	if(dataBase != NULL) {
		uint32_t data_addr = 0;
		uint32_t data_size = 0;
		getAddrSize(dataBase, &data_addr, &data_size);

		memory->dataMem->memBase = data_addr;
		if(data_size != 0) {
			memory->dataMem->memLen = data_size;
		}
		printf("Database = '%s' => 0x%08x:0x%08x\n", dataBase, data_addr, data_size);
	}

	if(hostBase != NULL) {
		printf("Host Address = %s - 0x%08x\n", hostBase, getValue(hostBase));
		hostAddr = getValue(hostBase);
	}

	if(!quiet) {
		printf(".text: %08x %08x\n", memory->insnMem->memBase, memory->insnMem->memLen);
		printf(".data: %08x %08x\n", memory->dataMem->memBase, memory->dataMem->memLen);
		printf("Host:  %08x %08x\n", hostAddr, hostSize);
	}

	if(!quiet) {
		printf("Elf file: %s\n", elfFile);
	}

	if(lflag) {
		const char* logfile = "RiscV.log";
		lfp = fopen(logfile, "w");
		printf("Logfile: %s\n", logfile);
	}

	//		Read and Process EFL File
	uint32_t entry = 0;
	readElf(elfFile, memory, cpu, heap_len, &entry);

	if(uflag) {
		memory->dataMem->elf32   = memory->insnMem->elf32;
	}

	if(!quiet) {
		printf(".text:\n");
		for(Elf32_Info* ip=memory->insnMem->elf32; ip != NULL; ip = ip->next) {
			printf("  %-12.12s 0x%08x  0x%08x\n", ip->name, ip->addr, ip->size);
		}
		printf(".data:\n");
		for(Elf32_Info* ip=memory->dataMem->elf32; ip != NULL; ip = ip->next) {
			printf("  %-12.12s 0x%08x  0x%08x\n", ip->name, ip->addr, ip->size);
		}
	}

	// get Length of the .text section
	uint32_t text_start = -1;
	uint32_t text_end = 0;
	for(Elf32_Info* ip=memory->insnMem->elf32; ip != NULL; ip = ip->next) {
		uint32_t len = ip->addr + ip->size;
		if(ip->addr < text_start) {
			text_start = ip->addr;
		}
		if(len > text_end) {
			text_end = len;
		}
	}
	if(!quiet) {
		printf(".text: start = 0x%08x, end = 0x%08x\n", text_start, text_end);
	}

	// get total Length of the .data section
	uint32_t data_start = -1;
	uint32_t data_end = 0;
	for(Elf32_Info* ip=memory->dataMem->elf32; ip != NULL; ip = ip->next) {
		uint32_t len = ip->addr + ip->size;
		if(ip->addr < data_start) {
			data_start = ip->addr;
		}
		if(len > data_end) {
			data_end = len;
		}
	}
	if(!quiet) {
		printf(".data: start = 0x%08x, end = 0x%08x\n", data_start, data_end);
	}

	if(dis_flag) {
		disasm_all(text_start, text_end);
	}

	if(c_flag) {
		// compile to Warp output
		rv_compile("rv_warp_out.cpp", 0x00000000, text_end);
	}

	if(!quiet) {
		printf("Ready to run: %s @ %08x, data_end = %08x\n", elfFile, entry, data_end);
	}

	cpu->reset();

	cpu->start(entry);
	printf("Entry @ 0x%08x: 0x%08x\n", entry, cpu->fetch(entry));

	cpu->edata(data_end);
	// cpu->edata(memory->dataMem->memBase + memory->dataMem->memLen);

	// set the stack to the top of data memory
	uint32_t sp_addr = memory->dataMem->memBase + memory->dataMem->memLen-16;
	cpu->setStack(sp_addr);
	if(!quiet) {
		printf("Stack Pointer: %08x\n", sp_addr);
	}

	cpu->set_trace(trace_flag);

	if(vflag) {
		verbose = 0;
		cpu->verbose = true;
	}
	if(quiet) {
		verbose = 0;
		cpu->verbose = false;
	}

	// cpu->verbose = true;

	// run ...
	if(debug_flag) {
		printf("Interactive Debug Mode:\n");
		debug_mode();
	}
	else {
		if(!quiet) {
			printf("Run Mode:\n");
		}
		cpu->run(-1);
	}

	cpu->dump_vliw();

	return 0;
}
