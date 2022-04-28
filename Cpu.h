#pragma once

// #include "riscv_csr.h"

typedef char i8_t;
typedef short i16_t;
typedef int i32_t;
typedef long long i64_t;
typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;

typedef unsigned long long addr64_t;
typedef unsigned long long data64_t;
typedef unsigned int data32_t;

#if RV64
#include "VliwTable_rv64.h"
#endif
#if RV32
#include "VliwTable_rv32.h"
#endif

const char* getVliwName(int n);
const char* getRegName(int n);

class Vliw {
public:
	uint8_t		opcode;
	uint8_t		rd;
	uint8_t		rs1;
	uint8_t		rs2;
	int32_t		imm;	// sign extends to 64 bit int by default

	Vliw(void) {
		memset(this, 0, sizeof(*this));
	};

	Vliw(int a0) {
		memset(this, 0, sizeof(*this));
		opcode=a0;
	};

	void dump(void) {
		printf("  op=%s (%02x), rd=%s, rs1=%s, rs2=%s, imm=%08x\n",
			getVliwName(opcode), opcode, getRegName(rd), getRegName(rs1), getRegName(rs2), imm);
	};
};

#define PC_MASK 0xfffffffe

#if RV64
typedef uint64_t data_t;
typedef uint64_t addr_t;
#endif
#if RV32
typedef	uint32_t data_t;
typedef	uint32_t addr_t;
#endif

class Cpu {
public:
	data_t		reg[32];
	addr_t		pc;
	addr_t		pc_inc;		// increment PC
	addr_t		pc_next;	// next PC; pc_inc or jmp/bra destination
	uint64_t	cycle;		// instruction cycle counter

	addr_t		breakpoint;

	addr_t		heap;		// brk(), sbrk() address
	addr_t		endOfData;	// brk(), sbrk() address

	// configuration
	unsigned endian;	// 0x00 = big, 0x03 = little
	// unsigned isaType;	// Instruction Set Architecture, ie: ISA_MIPS32R2

	// execution state
	bool running;
	bool stop;			// stop request
	bool quiet;
	int verbose;
	bool singleStep;
	bool irq;			// request for interrupt
	bool trace;

	// CSR Registers
	// Machine Trap Setup
	uint32_t mstatus;	// mstatus		Machine Status
	uint32_t mie;		// mie			Machine Interrupt Enable
	uint32_t mtvec;		// mtvec		Machine Trap Handler

	// Machine Trap Handling
	uint32_t mscratch; 	// mscratch		Machine Scratch Register
	uint32_t mepc;		// mepc			Machine Exception Program Counter
	uint32_t mtval;		// mtval		Machine Trap Value
	uint32_t mip;		// mip			Machine Interrupt Pending
	uint32_t mcause; 	// mcause		Machine Exception Cause

	Cpu(void) {
		memset(this, 0, sizeof(*this));
	};

	void reset(void) {
		memset(this, 0, sizeof(*this));
		breakpoint = -1;	// not a valid pc
		// brk_addr = 0;
		irq = false;
		verbose = 0;
	};

	// program start/entry address
	void start(uint64_t entry) {
		pc = entry;
		pc_next = pc;
	};

	// set brk() address
	void edata(uint64_t edata) {
		// printf("Set brk_addr: %08lx\n", edata);
		heap = 0;
		endOfData = edata;
	};

	void set_trace(bool t) {
		trace = t;
	};

	bool run(uint64_t n);
	bool execute(Vliw* vlp);
	void sigTick(void);

	void setPc(uint32_t target) {
		// pc = target & 0xfffffffc;
		pc_next = target & 0xfffffffc;
	};

	void setStack(uint32_t target) {
		reg[2] = target & 0xfffffffc;
	};

	bool checkPc(unsigned target) {
		if ((pc-memory->insnMem->memBase) >= memory->insnMem->memLen) return false;
		return true;
	};

	unsigned fetch() {
		return memory->insnMem->fetch32(pc);
	};

	unsigned fetch(uint32_t addr) {
		return memory->insnMem->fetch32(addr);
	};

	void csr_insn(int type, int csr, int rd, int rs, uint32_t imm32);
	void ecall(void);
	void ebreak(void);
	void mret(void);
	bool do_external_irq(void);
	bool do_time_irq(void);

	// newlib syscalls
	void syscall(void);
	uint32_t sys_brk(uint32_t a0, uint32_t a1);
	uint32_t sys_gettimeofday(uint32_t a0);
	int32_t sys_open(uint32_t pathname, uint32_t flags, uint32_t mode);
	int32_t sys_close(uint32_t fd);
	int sys_read(uint32_t a0, uint32_t a1, uint32_t a2);
	int sys_write(uint32_t a0, uint32_t a1, uint32_t a2);
	int sys_lseek(int file, int offset, int whence);
	int sys_link(uint32_t a0, uint32_t a1);
	int sys_unlink(uint32_t a0);

#if RV64
	void dumpReg32(void) {
		for(int i=0; i<32; ++i) {
			// printf("  %02d:%08x", i, (uint32_t)reg[i]);
			printf(" %4.4s:%016llx", getRegName(i), reg[i]);
			if((i&0x3) == 0x3) {
				printf("\n");
			}
		}
	};
#endif
#if RV32
	void dumpReg32(void) {
		for(int i=0; i<32; ++i) {
			// printf("  %02d:%08x", i, (uint32_t)reg[i]);
			printf(" %4.4s:%08x", getRegName(i), (uint32_t)reg[i]);
			if((i&0x7) == 0x7) {
				printf("\n");
			}
		}
	};
#endif

	void dump32(void) {
		printf("  [%s] pc:%08x, pc_next:%08x\n", hex64(cycle), (uint32_t)pc, (uint32_t)pc_next);
		dumpReg32();
	};

	void log(Vliw* vlp) {
		if(lfp) {
			// fprintf(lfp, "%08x => %08x\n", (uint32_t)pc, (uint32_t)pc_next);
			const char* opname = &getVliwName(vlp->opcode)[5];
			fprintf(lfp, "%08x: %s %s,%s,%s,0x%08x\n", pc, opname, getRegName(vlp->rd),
				getRegName(vlp->rs1), getRegName(vlp->rs2), vlp->imm);
			fflush(lfp);
			// printf("%08x: %s %s,%s,%s,0x%08x\n", pc, opname, getRegName(vlp->rd),
			//	getRegName(vlp->rs1), getRegName(vlp->rs2), vlp->imm);

		}
	};

	void checkAddr(uint32_t addr32) {
		uint32_t text_base = memory->insnMem->memBase;
		uint32_t text_end  = text_base+memory->insnMem->memLen;

		uint32_t data_base = memory->dataMem->memBase;
		uint32_t data_end  = data_base+memory->dataMem->memLen;

		if(!(addr32 >= data_base && addr32 < data_end)
			&& !(addr32 >= text_base && addr32 < text_end)
			&& !(addr32 >= 0xffffff00 && addr32 <= 0xfffffffc)) {
			// && addr32 != 0xfffffffc) {
			printf("checkAddr: Bad Address: %08x @ pc=%08x\n", addr32, (uint32_t)pc);
			dump32();
			dump_vliw();
			exit(-1);
		}
	};

	void dump_vliw(void);

};

#include "OpTable.h"
extern Optable optable[];

extern Cpu* cpu;
void Sleep(int waitTime);
bool decode2vliw(uint32_t addr, uint32_t code, Vliw* vlp);
bool decodeCompressed(uint32_t addr, uint32_t code, Vliw* vlp);
