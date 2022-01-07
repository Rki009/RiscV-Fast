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

int Version = 0;
int Release = 2;

// options
bool doSignature = false;
bool quiet = false;			// run quite -- very minimal output


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

void uartWrite(uint32_t value) {
	uint8_t c = value;
	// printf("Uart: '%c' (0x%02x)\n", isprint(c)?c:'.', c);
#if 0
	printf("%c", c);
	fflush(stdout);
#else
	fprintf(stderr, "%c", c);
	fflush(stderr);
#endif
	static FILE* ofp;
	if(ofp == NULL) {
		ofp = fopen("stdout.txt", "w");
	}
	fputc(c, ofp);
	fflush(ofp);
	return;
};


#if 0
void ioWrite32(uint32_t addr, uint32_t value) {
	if(addr == 0xfffffffc) {
		uartWrite(value);
	}
}
#endif


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
#if 0
		int c = (uint8_t)value;
		printf("Uart: '%c' [%02x]\n", isprint(c)?c:'.', c);
#else
		uartWrite(value);
#endif
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


void usage(void) {
	printf("	-t addr		- .text base addresss\n");
	printf("	-d addr		- .data base addresss\n");
	printf("	-h addr		- host base addresss\n");
	printf("	-v			- verbose\n");
	printf("	-q			- quiet\n");
	printf("	-u			- unified memory, .text/.data same address space\n");
};


int main(int argc, char** argv) {
	bool vflag = 0;			// verbose
	bool lflag = 0;			// logfile
	bool uflag = 0;			// unified memory
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
				printf("Newlib = %d\n", do_newlib);
			}
			else if(strcmp(argv[index], "--core")==0) {
				do_core = true;
				printf("Do_core = %d\n", do_core);
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
	while((c = getopt(argc, argv, "lvqub:t:d:h:")) != -1) {
		// printf("c = '%c'\n", c);

		switch(c) {
		case '-':
			printf("Option: %s\n", optarg);
			vflag = 0;
			break;

		// quiet flag
		case 'q':
			quiet = true;
			vflag = 0;
			break;

		case 'v':
			vflag = 1;
			break;

		// -u, unified memory
		case 'u':
			uflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		// case 'b':
		//	breakStr = optarg;
		//	break;

		// -t addr, .text base address
		case 't':
			textBase = optarg;
			break;
		// -d addr, .data base address
		case 'd':
			dataBase = optarg;
			break;
		// -h addr, host base address
		case 'h':
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
		printf("Non-option argument %s\n", argv[index]);
		if(index == optind) {
			elfFile = argv[index];
		}
	}
#if 0
	if(argc >1) {
		elfFile = argv[1];
	}
#endif

	if(!quiet) {
		printf("MySim - RV32/Rv64\n");
		printf("  (C) Ron K. Irvine, 2020.\n");
		// printf("  Build: %s %s\n", __DATE__, __TIME__);
		printf("  Build: V%d.%02d, %s\n", Version, Release, buildDate());
	}

	if(textBase != NULL) {
		// printf("Textbase = %s - 0x%08x\n", textBase, getValue(textBase));
		memory->insnMem->memBase = getValue(textBase);
	}
	if(dataBase != NULL) {
		// printf("Database = %s - 0x%08x\n", dataBase, getValue(dataBase));
		memory->dataMem->memBase = getValue(textBase);
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
	if(uflag) {
		memory->dataMem = memory->insnMem;
		if(!quiet) {
			printf("Unified memory!\n");
		}
	}

	if(!quiet) {
		printf("Elf file: %s\n", elfFile);
	}

	if(lflag) {
		const char* logfile = "RiscV.log";
		lfp = fopen(logfile, "w");
		printf("Logfile: %s\n", logfile);
	}

	// unsigned long long entry;
	// unsigned long long edata;
	uint32_t entry;
	uint32_t edata;

	readElf(elfFile, memory->insnMem, memory->dataMem, &entry, &edata);

	if(!quiet) {
		printf("Ready to run: %s @ %08x, %08x\n", elfFile, entry, edata);
	}

	cpu->reset();
	cpu->start(entry);
	cpu->edata(edata);

	// set the stack to the top of data memory
	cpu->setStack(memory->dataMem->memBase + memory->dataMem->memLen-16);

	// cpu->breakpoint = 0x00000058; // after memset
	// cpu->breakpoint = 0x00000000; // main()
	// cpu->breakpoint = 0x000003d24; // <__libc_fini_array>

	// cpu->breakpoint = 0x000000f4; // Proc2() -- end of Dhry loop
	// cpu->breakpoint = 0x00000160; // Proc5() -- begin of Dhry loop
	// cpu->breakpoint = 0x00000418; 	// Proc0() -- begin of Dhry
	// cpu->breakpoint = 0x0000057a; 	// Memset

	if(vflag) {
		verbose = 0;
		cpu->verbose = true;
	}
	if(quiet) {
		verbose = 0;
		cpu->verbose = false;
	}

	cpu->run(-1);


	return 0;
}
