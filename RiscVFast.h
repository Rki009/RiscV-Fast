// extern unsigned char mem[0x10000];		// memory

#pragma once

// Fastest possible
#define VERY_FAST

#define DO_MUL_DIV 1

// prototypes
extern FILE* lfp;
void uartSend(char c);
void initSegment(char* filename);
void initData(char* filename);

void fatal(const char* fmt, ...);
void error(const char* fmt, ...);
void warn(const char* fmt, ...);
#define trace(...) _trace(__FILE__, __FUNC__, __LINE__, __VA_ARGS__)
void _trace(const char* file, const char* func, int line, const char* fmt, ...);

extern void disasm(uint32_t pc, uint32_t insn, char* buf);
void rv_insn(uint32_t pc, uint32_t insn);
const char* csr_name(int csr);
void rv_compile(const char* outfile, uint32_t addr, uint32_t len);


// format uint64_t as hex or decimal
const char* hex64(uint64_t value);
const char* dec64(uint64_t value);


// runtime execution options
extern bool do_newlib;		// process newlib syscalls
extern bool do_core;		// allows write to instruction memory

// void ioWrite32(uint32_t addr, uint32_t value);

extern bool doSignature;
extern bool quiet;			// run quite -- very minimal output


#define ROM_BASE 	0x00000000
#define ROM_SIZE 	0x00100000		// 1 MB, Must be a power of 2
#define ROM_MASK 	(ROM_SIZE-1)

#define SRAM_BASE 	0x40000000
#define SRAM_SIZE 	0x00100000		// 1 MB, Must be a power of 2
#define SRAM_MASK 	(SRAM_SIZE-1)

#define HOST_BASE 	0xffff0000
#define HOST_SIZE 	0x00010000		// 64 K, Must be a power of 2
#define HOST_MASK 	(ROM_SIZE-1)

// #define DO_MEMORY_CHECK 1


// Fake devices and Host
extern uint32_t	hostAddr;	// 0xffff0000
extern uint32_t	hostSize;	// 64K
uint32_t host(bool rw, uint32_t addr, uint32_t value);
inline void host_write(uint32_t addr, uint32_t value) {
	host(true, addr, value);
	return;
};
inline uint32_t host_read(uint32_t addr) {
	return host(false, addr, 0);
};

/*
typedef struct {
	Elf32_Word	sh_name;		// name of the section
	Elf32_Word	sh_type;		// member category
	Elf32_Word	sh_flags;		// attribute  flags
	Elf32_Addr	sh_addr;		// physical address in bytes
	Elf32_Off	sh_offset;		// file offset in bytes
	Elf32_Word	sh_size;		// section size in bytes
	Elf32_Word	sh_link;		// section header table index link
	Elf32_Word	sh_info;		// extra info
	Elf32_Word	sh_addralign;	// alignment constraint; 1 or 0
	Elf32_Word	sh_entsize;		// entry pointer
} Elf32_Shdr;
*/
class Elf32_Info {
public:
	Elf32_Info*	next;		// link list
	char* 		name;		// name of the section
	uint32_t	type;		// member category
	uint32_t	flags;		// attribute  flags
	uint32_t	addr;		// physical address in bytes
	uint32_t	size;		// section size in bytes
	uint64_t	offset;		// file offset in bytes

	Elf32_Info(void) {
		memset(this, 0, sizeof(Elf32_Info));
		next = NULL;
	};
};

// typedef uint32_t addr_t;
// typedef uint32_t data_t;

class Segment {
	// protected:
public:
	char* 		memName;
	uint32_t	memBase;
	uint32_t	memLen;
	uint32_t	memMask;
	Elf32_Info*		elf32;
	uint8_t*	data;

public:
	Segment(const char* name, uint32_t base, uint32_t size) {
		memBase = base;
		memLen = size;
		memMask = (size-1);
		memName = strdup(name);

		data = (uint8_t*)malloc(memLen);
		if(data == NULL) {
			fatal("Segment: alloc()\n");
			exit(-1);
		}
		printf("%s: 0x%08x, 0x%08x, 0x%08x\n", memName, memBase, memLen, memMask);
	};

	bool ok(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			return false;
		}
		return true;
	};

#ifdef DO_MEMORY_CHECK
	uint8_t* getPtr(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			return NULL;
		}
		return (uint8_t*)(&data[addr]);
	};

	uint32_t fetch32(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		uint32_t* dp = (uint32_t*)(&data[addr]);
		return *dp;
	};

	uint64_t read64(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		uint64_t* dp = (uint64_t*)(&data[addr]);
		return *dp;
	};

	uint32_t read32(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		uint32_t* dp = (uint32_t*)(&data[addr]);
		return *dp;
	};

	uint32_t read16(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		int16_t* dp = (int16_t*)(&data[addr]);
		return *dp;
	};

	uint32_t read16u(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		uint16_t* dp = (uint16_t*)(&data[addr]);
		return *dp;
	};

	uint32_t read8(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		int8_t* dp = (int8_t*)(&data[addr]);
		return *dp;
	};

	uint32_t read8u(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return(0);
		}
		uint8_t* dp = (uint8_t*)(&data[addr]);
		return *dp;
	};

	void write64(uint32_t addr, uint64_t value) {
		// printf("Write64: 0x%08x <= 0x%08llx\n", memBase+addr, value);
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return;
		}
		uint64_t* dp = (uint64_t*)(&data[addr]);
		*dp = value;
		return;
	};

	void write32(uint32_t addr, uint32_t value) {
		// printf("Write32: 0x%08x <= 0x%08x\n", memBase+addr, value);
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return;
		}
		uint32_t* dp = (uint32_t*)(&data[addr]);
		*dp = value;
	};

	void write16(uint32_t addr, uint32_t value) {
		// printf("Write32: 0x%08x <= 0x%08x\n", memBase+addr, value);
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return;
		}
		uint16_t* dp = (uint16_t*)(&data[addr]);
		*dp = value;
		return;
	};

	void write8(uint32_t addr, uint32_t value) {
		// printf("Write32: 0x%08x <= 0x%08x\n", memBase+addr, value);
		addr -= memBase;
		if(addr >= memLen) {
			fatal("Bad memory access 0x%08x\n", addr);
			return;
		}
		uint8_t* dp = (uint8_t*)(&data[addr]);
		*dp = value;
		return;
	};
#else
	uint8_t* getPtr(uint32_t addr) {
		return (uint8_t*)(&data[addr&memMask]);
	};

	uint32_t fetch32(uint32_t addr) {
		uint32_t* dp = (uint32_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint64_t read64(uint32_t addr) {
		uint64_t* dp = (uint64_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint32_t read32(uint32_t addr) {
		uint32_t* dp = (uint32_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint32_t read16(uint32_t addr) {
		int16_t* dp = (int16_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint32_t read16u(uint32_t addr) {
		uint16_t* dp = (uint16_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint32_t read8(uint32_t addr) {
		int8_t* dp = (int8_t*)(&data[addr&memMask]);
		return *dp;
	};

	uint32_t read8u(uint32_t addr) {
		uint8_t* dp = (uint8_t*)(&data[addr&memMask]);
		return *dp;
	};

	void write64(uint32_t addr, uint64_t value) {
		// printf("Write64: 0x%08x <= 0x%08llx\n", memBase+addr, value);
		uint64_t* dp = (uint64_t*)(&data[addr&memMask]);
		*dp = value;
		return;
	};

	void write32(uint32_t addr, uint32_t value) {
		uint32_t* dp = (uint32_t*)(&data[addr&memMask]);
		*dp = value;
	};

	void write16(uint32_t addr, uint32_t value) {
		// printf("Write32: 0x%08x <= 0x%08x\n", memBase+addr, value);
		uint16_t* dp = (uint16_t*)(&data[addr&memMask]);
		*dp = value;
		return;
	};

	void write8(uint32_t addr, uint32_t value) {
		// printf("Write32: 0x%08x <= 0x%08x\n", memBase+addr, value);
		uint8_t* dp = (uint8_t*)(&data[addr&memMask]);
		*dp = value;
		return;
	};
#endif
};


class Memory {
public:
	Segment* insnMem;
	Segment* dataMem;

	Memory(void) {
		insnMem = new Segment("Code", ROM_BASE, ROM_SIZE);
		dataMem = new Segment("Data", SRAM_BASE, SRAM_SIZE);
	};

	int isdata(uint32_t addr) {
		return ((addr>>28) == (SRAM_BASE>>28));
	};
	int istext(uint32_t addr) {
		return ((addr>>28) == (ROM_BASE>>28));
	};

	int ishost(uint32_t addr) {
		return (addr >= hostAddr && addr <= (hostAddr+(hostSize-1)));
	};

	uint8_t* getPtr(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->getPtr(addr);
		}
		if(istext(addr)) {
			return insnMem->getPtr(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return NULL;
	};

	uint32_t fetch32(uint32_t addr) {
		if(istext(addr)) {
			return insnMem->fetch32(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint64_t read64(uint64_t addr) {
		if(isdata(addr)) {
			return dataMem->read64(addr);
		}
		if(istext(addr)) {
			return insnMem->read64(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read32(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->read32(addr);
		}
		if(istext(addr)) {
			return insnMem->read32(addr);
		}
		if(ishost(addr)) {
			// uint32_t data = 0;
			// printf("Host: Read32 <= 0x%08x\n", data);
			return host_read(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read16(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->read16(addr);
		}
		if(istext(addr)) {
			return insnMem->read16(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read16u(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->read16u(addr);
		}
		else if(istext(addr)) {
			return insnMem->read16u(addr);
		}
		else {
			fatal("Bad memory access 0x%08x\n", addr);
		}
		return 0;
	};

	uint32_t read8(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->read8(addr);
		}
		if(istext(addr)) {
			return insnMem->read8(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read8u(uint32_t addr) {
		if(isdata(addr)) {
			return dataMem->read8u(addr);
		}
		if(istext(addr)) {
			return insnMem->read8u(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	void write64(uint32_t addr, uint64_t value) {
		if(isdata(addr)) {
			dataMem->write64(addr, value);
			return;
		}
		if(do_core && istext(addr)) {
			insnMem->write64(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};

	void write32(uint32_t addr, uint32_t value) {
		if(isdata(addr)) {
			dataMem->write32(addr, value);
			return;
		}
		if(do_core && istext(addr)) {
			insnMem->write32(addr, value);
			return;
		}
		if(ishost(addr)) {
			host_write(addr, value);
			return;
		}
		fatal("write32: Bad memory access 0x%08x\n", addr);
	};
	void write16(uint32_t addr, uint32_t value) {
		if(isdata(addr)) {
			dataMem->write16(addr, value);
			return;
		}
		if(do_core && istext(addr)) {
			insnMem->write16(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};
	void write8(uint32_t addr, uint32_t value) {
		if(isdata(addr)) {
			dataMem->write8(addr, value);
			return;
		}
		if(do_core && istext(addr)) {
			insnMem->write8(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};

};

extern Memory* memory;
extern int verbose;

int readElf(const char* inputFilename, Segment* code, Segment* data, uint32_t* entry=NULL);

// extern unsigned char insnMem[ROM_SIZE];		// Program memory
// extern unsigned char dataMem[SRAM_SIZE];	// SRAM memory

// extern Segment* insnMem;
// extern Segment* dataMem;




