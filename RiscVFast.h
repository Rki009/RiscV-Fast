// extern unsigned char mem[0x10000];		// memory

#pragma once

// Fastest possible
#define VERY_FAST


// prototypes
extern FILE* lfp;
void uartSend(char c);
void initSegment(char* filename);
void initData(char* filename);
void fatal(const char* fmt, ...);
#define trace(...) _trace(__FILE__, __FUNC__, __LINE__, __VA_ARGS__)
void _trace(const char* file, const char* func, int line, const char* fmt, ...);

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
#define ROM_SIZE 	0x00100000		// 1 MB
#define SRAM_BASE 	0x40000000
#define SRAM_SIZE 	0x00100000		// 1 MB
#define HOST_BASE 	0xffff0000
#define HOST_SIZE 	0x00010000		// 64 K


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


// typedef uint32_t addr_t;
// typedef uint32_t data_t;

class Segment {
	// protected:
public:
	uint32_t	memBase;
	uint32_t	memLen;
	uint8_t*	data;

public:
	Segment(uint32_t base, uint32_t size) {
		memBase = base;
		memLen = size;
		data = (uint8_t*)malloc(memLen);
		if(data == NULL) {
			fatal("Segment: alloc()\n");
			exit(-1);
		}
		// printf("Segment: 0x%08x, 0x%08x\n", memBase, memLen);
	};

	bool ok(uint32_t addr) {
		addr -= memBase;
		if(addr >= memLen) {
			return false;
		}
		return true;
	};

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
		return;
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
};


class Memory {
public:
	Segment* insnMem;
	Segment* dataMem;

	Memory(void) {
		insnMem = new Segment(ROM_BASE, ROM_SIZE);
		dataMem = new Segment(SRAM_BASE, SRAM_SIZE);
	};

	uint8_t* getPtr(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->getPtr(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->getPtr(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return NULL;
	};

	uint32_t fetch32(uint32_t addr) {
		if(insnMem->ok(addr)) {
			return insnMem->fetch32(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint64_t read64(uint64_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read64(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->read64(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read32(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read32(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->read32(addr);
		}
		if(addr >= hostAddr && addr < (hostAddr+(hostSize-1))) {
			// uint32_t data = 0;
			// printf("Host: Read32 <= 0x%08x\n", data);
			return host_read(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read16(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read16(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->read16(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read16u(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read16u(addr);
		}
		else if(insnMem->ok(addr)) {
			return insnMem->read16u(addr);
		}
		else {
			fatal("Bad memory access 0x%08x\n", addr);
		}
		return 0;
	};

	uint32_t read8(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read8(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->read8(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	uint32_t read8u(uint32_t addr) {
		if(dataMem->ok(addr)) {
			return dataMem->read8u(addr);
		}
		if(insnMem->ok(addr)) {
			return insnMem->read8u(addr);
		}
		fatal("Bad memory access 0x%08x\n", addr);
		return 0;
	};

	void write64(uint32_t addr, uint64_t value) {
		if(dataMem->ok(addr)) {
			dataMem->write64(addr, value);
			return;
		}
		if(do_core && insnMem->ok(addr)) {
			insnMem->write64(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};

	void write32(uint32_t addr, uint32_t value) {
		if(dataMem->ok(addr)) {
			dataMem->write32(addr, value);
			return;
		}
		if(do_core && insnMem->ok(addr)) {
			insnMem->write32(addr, value);
			return;
		}
		if(addr >= hostAddr && addr <= (hostAddr+(hostSize-1))) {
			host_write(addr, value);
			return;
		}
		fatal("write32: Bad memory access 0x%08x\n", addr);
	};
	void write16(uint32_t addr, uint32_t value) {
		if(dataMem->ok(addr)) {
			dataMem->write16(addr, value);
			return;
		}
		if(do_core && insnMem->ok(addr)) {
			insnMem->write16(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};
	void write8(uint32_t addr, uint32_t value) {
		if(dataMem->ok(addr)) {
			dataMem->write8(addr, value);
			return;
		}
		if(do_core && insnMem->ok(addr)) {
			insnMem->write8(addr, value);
			return;
		}
		fatal("Bad memory access 0x%08x\n", addr);
	};

};

extern Memory* memory;
extern int verbose;

int readElf(const char* inputFilename, Segment* code, Segment* data,
	uint32_t* entry=NULL, uint32_t* edata=NULL);

// extern unsigned char insnMem[ROM_SIZE];		// Program memory
// extern unsigned char dataMem[SRAM_SIZE];	// SRAM memory

// extern Segment* insnMem;
// extern Segment* dataMem;




