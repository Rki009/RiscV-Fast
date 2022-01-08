#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <sys/time.h>

#include "RiscVFast.h"
#include "Cpu.h"
#include "Newlib-syscall.h"

#include "riscv_csr.h"

// prototype
int rand32(uint32_t* value);


#if RV64
#include "VliwTable_rv64.cpp"
#endif
#if RV32
#include "VliwTable_rv32.cpp"
#endif

FILE* tfp = NULL;


#ifdef __linux__
void Sleep(int ms = 1) {
	;
}
#endif


const char* getVliwName(int n) {
	if(n < 0 || n >= VLIW_SIZE) {
		return "???";
	}
	return VliwName[n];
}

Vliw vliw_insn[0x100000];	// 1M instructions

Vliw* vliw_nop = new Vliw(VLIW_NOP);

bool Cpu::run(uint64_t n) {
	Vliw* vlp = new Vliw;

	running = true;
	cycle = 0;
	// memset(vliw_insn, 0, sizeof(vliw_insn));

	if(verbose) {
		printf("Run - %s\n", dec64(n));
	}

	while(running) {
		if(n-- == 0) {
			break;
		}



#ifndef VERY_FAST
		// if((pc&0x00000003) != 0) {
		if((pc&0x00000001) != 0) {
			// printf("Unaligned PC: %08llx\n", pc);
			printf("Unaligned PC: %08x\n", (uint32_t)pc);
			Sleep(300);
			return false;
		}
		if(checkPc(pc) != true) {
			// printf("Bad PC: %08llx\n", pc);
			printf("Bad PC: %08x\n", (uint32_t)pc);
			dump32();
			Sleep(300);
			return false;
		};
#endif

#ifndef VERY_FAST
		// check for breakpoint
		if(pc == breakpoint || singleStep) {
			char buf[64];
			if(singleStep) {
				printf("Single Step @ 0x%08x\n", (uint32_t)pc);
			}
			else {
				printf("BreakPoint @ 0x%08x\n", (uint32_t)pc);
			}
again:
			fgets(buf, sizeof(buf), stdin);
			if(strcmp(buf, "exit\n")==0) {
				return true;
			}
			else if(strcmp(buf, "s\n")==0 || strcmp(buf, "\n")==0) {
				singleStep = true;
			}
			else if(strcmp(buf, "r\n")==0 || strcmp(buf, "run\n")==0) {
				singleStep = false;
			}
			else if(strcmp(buf, "v\n")==0 || strcmp(buf, "+\n")==0 || strcmp(buf, "verbose\n")==0) {
				++verbose;
				printf("Verbose = %d\n", verbose);
				cpu->dump32();
				goto again;
			}
			else if(strcmp(buf, "-\n")==0) {
				verbose = 0;
				printf("Verbose = %d\n", verbose);
				goto again;
			}
		}
#endif

#ifdef VERY_FAST
		pc_inc = pc+4;	// full 32 bit
		// lookup the instruction in the vliw cache, if ok ... use it
		uint32_t vliw_index = (pc>>2);
		// Vliw* vcp = &vliw_insn[vliw_index];
		Vliw* vcp = &vliw_insn[vliw_index];
		if(vcp->opcode == VLIW_NONE) {
			unsigned code = fetch();		// do not advance the pc
			decode(pc, code, vcp);
		}
		vlp = vcp;
#else
		unsigned code = fetch();		// do not advance the pc
		// unsigned code = memory->fetch32(pc);		// do not advance the pc
		if((code&0x00000003) == 0x00000003) {
			pc_inc = pc+4;	// full 32 bit
#if 0
			decode(pc, code, vlp);
#else
			// lookup the instruction in the vliw cache, if ok ... use it
			uint32_t vliw_index = (pc>>1);
			// Vliw* vcp = &vliw_insn[vliw_index];
			Vliw* vcp = &vliw_insn[vliw_index];
			if(vcp->opcode == VLIW_NONE) {
				decode(pc, code, vcp);
			}
			vlp = vcp;
#endif
		}
		else  {
			pc_inc = pc+2;	// 16 bit
			code &= 0x0000ffff;
			decodeCompressed(pc, code, vlp);
		}
#endif

		pc_next = pc_inc;

#ifndef VERY_FAST
		if(verbose) {
			vlp->dump();
		}
		if(lfp) {
			cpu->log(vlp);
		}
#endif

		// check for interrupts
		//	do this every 16 cycles just to reduce
		//	the check loading on execution time
		if((cycle&0xf) == 0xf) {
			if(do_external_irq()) {
				vlp = vliw_nop;
			}
			if(do_time_irq()) {
				vlp = vliw_nop;
			}

			// Fake Time Interrupt, 100 Hz
			// Interrupt about every 1M cycles
			// If we are doing about 80MHz then we interrupt about 100 times a second
			// Of course this is all very rough calculations and is dependent on the
			// Host processor speed and other stuff
			if((cycle&0xfffff) == 0xfffff) {
				mip |= RISCV_MIP_MTIP;	// set time interrupt pending
				// printf("**** SET TIME IRQ ***** - mip = %08x\n", mip);
			}
		}

		//Execute the instruction
		cpu->execute(vlp);

#ifndef VERY_FAST
		if(verbose) {
			cpu->dump32();
		}

		if(doSignature) {
			sigTick();
		}
#endif

		// Next PC
		pc = pc_next;
		++cycle;

		if(stop) {
			stop = false;
			break;
		}
	}
	return true;
};

void Cpu::sigTick(void) {
	static FILE* sigFp = NULL;
	static data_t sav[32];

	if(sigFp == NULL) {
		sigFp = fopen("signature.txt", "w");
	}
	bool flag = false;
	for(int i=1; i<32; ++i) {
		if(reg[i] != sav[i]) {
			flag = true;
			break;
		}
	}

	if(flag) {
		// fprintf(sigFp, "%08x", (uint32_t)pc);
		for(int i=1; i<32; ++i) {
#if RV64
			if(reg[i] != sav[i]) {
				fprintf(sigFp, " %s:%016x", getRegName(i), reg[i]);
			}
#endif
#if RV32
			if(reg[i] != sav[i]) {
				fprintf(sigFp, " %s:%08x", getRegName(i), reg[i]);
			}
#endif
		}
		fprintf(sigFp, "\n");
	}

	for(int i=1; i<32; ++i) {
		sav[i] = reg[i];
	}
};



bool Cpu::execute(Vliw* vlp) {
	// uint64_t addr64;
	uint32_t addr32;
	uint32_t temp32;

	switch(vlp->opcode) {
	case VLIW_NOP:
		break;

	case VLIW_ILLEGAL:
		fatal("Illegal instruction @ 0x%08x\n", (uint32_t)pc);
		break;


	//-----------------------------------------------------------------
	//		Jump
	//-----------------------------------------------------------------
	case VLIW_J:
		pc_next = pc + vlp->imm;
		break;

	case VLIW_JAL:
		reg[vlp->rd] = pc_next;
		pc_next = pc + vlp->imm;
		break;

	case VLIW_JR:
		pc_next = reg[vlp->rs1];
		break;

	case VLIW_JALR:
		temp32 = pc_next;
		pc_next = reg[vlp->rs1] + vlp->imm;
		reg[vlp->rd] = temp32;
		break;


	//-----------------------------------------------------------------
	//		Branch
	//-----------------------------------------------------------------
	case VLIW_BEQ:
		if(reg[vlp->rs1] == reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BNE:
		if(reg[vlp->rs1] != reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

#if RV32
	case VLIW_BGE:
		if((int32_t)reg[vlp->rs1] >= (int32_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BGEU:
		if((uint32_t)reg[vlp->rs1] >= (uint32_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BLT:
		if((int32_t)reg[vlp->rs1] < (int32_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BLTU:
		if((uint32_t)reg[vlp->rs1] < (uint32_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;
#endif
#if RV64
	case VLIW_BGE:
		if((int64_t)reg[vlp->rs1] >= (int64_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BGEU:
		if((uint64_t)reg[vlp->rs1] >= (uint64_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BLT:
		if((int64_t)reg[vlp->rs1] < (int64_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;

	case VLIW_BLTU:
		if((uint64_t)reg[vlp->rs1] < (uint64_t)reg[vlp->rs2]) {
			pc_next = pc + vlp->imm;
		}
		break;
#endif

	//-----------------------------------------------------------------
	//		ALU
	//-----------------------------------------------------------------
	case VLIW_ADD:
		reg[vlp->rd] = reg[vlp->rs1] + reg[vlp->rs2];
		break;

	case VLIW_SUB:
		reg[vlp->rd] = reg[vlp->rs1] - reg[vlp->rs2];
		break;

	case VLIW_XOR:
		reg[vlp->rd] = reg[vlp->rs1] ^ reg[vlp->rs2];
		break;

	case VLIW_OR:
		reg[vlp->rd] = reg[vlp->rs1] | reg[vlp->rs2];
		break;

	case VLIW_AND:
		reg[vlp->rd] = reg[vlp->rs1] & reg[vlp->rs2];
		break;

	case VLIW_MUL:
		reg[vlp->rd] = reg[vlp->rs1] * reg[vlp->rs2];
		break;

	case VLIW_DIV:
		reg[vlp->rd] = reg[vlp->rs1] / reg[vlp->rs2];
		break;

	case VLIW_ADDI:
		reg[vlp->rd] = reg[vlp->rs1] + vlp->imm;
		break;

	case VLIW_XORI:
		reg[vlp->rd] = reg[vlp->rs1] ^ vlp->imm;
		break;

	case VLIW_ORI:
		reg[vlp->rd] = reg[vlp->rs1] | vlp->imm;
		break;

	case VLIW_ANDI:
		reg[vlp->rd] = reg[vlp->rs1] & vlp->imm;
		break;


	//-----------------------------------------------------------------
	//		Set on conditional
	//-----------------------------------------------------------------

	// SLTI - write 1 to rd if rs1 < imm, 0 otherwise; signed compare
	case VLIW_SLTI:
		reg[vlp->rd] = ((int32_t)reg[vlp->rs1] < (int32_t)vlp->imm)?1:0;
		break;

	// SLTUI - write 1 to rd if rs1 < imm, 0 otherwise; unsigned compare
	case VLIW_SLTIU:
		reg[vlp->rd] = ((uint32_t)reg[vlp->rs1] < (uint32_t)vlp->imm)?1:0;
		break;

#if RV32
	// SLT - write 1 to rd if rs1 < rs2, 0 otherwise; signed compare
	case VLIW_SLT:
		reg[vlp->rd] = ((int32_t)reg[vlp->rs1] < (int32_t)reg[vlp->rs2])?1:0;
		break;

	// SLTU - write 1 to rd if rs1 < rs2, 0 otherwise; unsigned compare
	case VLIW_SLTU:
		reg[vlp->rd] = ((uint32_t)reg[vlp->rs1] < (uint32_t)reg[vlp->rs2])?1:0;
		break;
#endif


	//-----------------------------------------------------------------
	//		Shifts
	//-----------------------------------------------------------------
	case VLIW_SRL:
		reg[vlp->rd] = (uint32_t)reg[vlp->rs1] >> (reg[vlp->rs2]&0x1f);
		break;

	case VLIW_SRA:
		reg[vlp->rd] = (int32_t)reg[vlp->rs1] >> (reg[vlp->rs2]&0x1f);
		break;

#if RV32
	case VLIW_SLL:
		reg[vlp->rd] = (uint32_t)reg[vlp->rs1] << (reg[vlp->rs2]&0x1f);
		break;

	case VLIW_SLLI:
		reg[vlp->rd] = (uint32_t)reg[vlp->rs1] << (vlp->imm&0x1f);
		break;
	case VLIW_SRLI:
		reg[vlp->rd] = (uint32_t)reg[vlp->rs1] >> (vlp->imm&0x1f);
		break;

	case VLIW_SRAI:
		reg[vlp->rd] = (int32_t)reg[vlp->rs1] >> (vlp->imm&0x1f);
		break;
#endif

	//-----------------------------------------------------------------
	//		LUI/AUIPC
	//-----------------------------------------------------------------
	case VLIW_LUI:
		reg[vlp->rd] = (vlp->imm<<12);
		break;

	case VLIW_AUIPC:
		reg[vlp->rd] = pc + (vlp->imm<<12);
		break;


	//-----------------------------------------------------------------
	//		Load
	//-----------------------------------------------------------------
	case VLIW_LW:
		addr32 = reg[vlp->rs1] + vlp->imm;
		checkAddr(addr32);
		reg[vlp->rd] = memory->read32(addr32);
		if(0 || cpu->verbose) {
			printf("lw: 0x%08x => 0x%08x (%s)\n", addr32, (uint32_t)reg[vlp->rd], getRegName(vlp->rd));
		}
		break;

	case VLIW_LH:
		addr32 = reg[vlp->rs1] + vlp->imm;
		checkAddr(addr32);
		reg[vlp->rd] = memory->read16(addr32);
		if(0 || cpu->verbose) {
			printf("lh: 0x%08x => 0x%08x (%s)\n", addr32, (int16_t)reg[vlp->rd], getRegName(vlp->rd));
		}
		break;

	case VLIW_LHU:
		addr32 = reg[vlp->rs1] + vlp->imm;
		checkAddr(addr32);
		reg[vlp->rd] = memory->read16u(addr32);
		if(0 || cpu->verbose) {
			printf("lhu: 0x%08x => 0x%08x (%s)\n", addr32, (uint16_t)reg[vlp->rd], getRegName(vlp->rd));
		}
		break;

	case VLIW_LB:
		addr32 = reg[vlp->rs1] + vlp->imm;
		checkAddr(addr32);
		reg[vlp->rd] = memory->read8(addr32);
		if(0 || cpu->verbose) {
			printf("lb: 0x%08x => 0x%08x (%s)\n", addr32, (int8_t)reg[vlp->rd], getRegName(vlp->rd));
		}
		break;

	case VLIW_LBU:
		addr32 = reg[vlp->rs1] + vlp->imm;
		checkAddr(addr32);
		reg[vlp->rd] = memory->read8u(addr32);
		if(0 || cpu->verbose) {
			printf("lbu: 0x%08x => 0x%08x (%s)\n", addr32, (uint8_t)reg[vlp->rd], getRegName(vlp->rd));
		}
		break;


	//-----------------------------------------------------------------
	//		Store
	//-----------------------------------------------------------------
	case VLIW_SW:
		addr32 = reg[vlp->rs1] + vlp->imm;
		memory->write32(addr32, (uint32_t)reg[vlp->rs2]);
		if(0 || cpu->verbose) {
			printf("sw: 0x%08x <= 0x%08x (%s)\n", addr32, (uint32_t)reg[vlp->rs2], getRegName(vlp->rs2));
		}
		break;

	case VLIW_SH:
		addr32 = reg[vlp->rs1] + vlp->imm;
		memory->write16(addr32, (uint32_t)reg[vlp->rs2]);
		if(0 || cpu->verbose) {
			printf("sh: 0x%08x <= 0x%04x (%s)\n", addr32, (uint32_t)reg[vlp->rs2]&0x0000ffff, getRegName(vlp->rs2));
		}
		break;

	case VLIW_SB:
		addr32 = reg[vlp->rs1] + vlp->imm;
		memory->write8(addr32, (uint32_t)reg[vlp->rs2]);
		if(0 || cpu->verbose) {
			printf("sb: 0x%08x <= 0x%02x (%s)\n", addr32, (uint32_t)reg[vlp->rs2]&0x000000ff, getRegName(vlp->rs2));
		}
		break;


	//-----------------------------------------------------------------
	//		System
	//-----------------------------------------------------------------
	case VLIW_ECALL:
		if(do_newlib) {
			syscall();
		}
		else {
			ecall();
		}
		break;

	case VLIW_EBREAK:
		ebreak();
		break;

	case VLIW_SRET:
		// NOP for now
		break;

	case VLIW_MRET:
		mret();
		break;

	case VLIW_WFI:
		// NOP for now
		break;



	//-----------------------------------------------------------------
	//		Control Registers
	//-----------------------------------------------------------------
	case VLIW_CSRRW:
		csr_insn(CSR_TYPE_W, vlp->imm&0xfff, vlp->rd, vlp->rs1, (uint32_t)reg[vlp->rs1]);
		break;
	case VLIW_CSRRS:
		csr_insn(CSR_TYPE_S, vlp->imm&0xfff, vlp->rd, vlp->rs1, (uint32_t)reg[vlp->rs1]);
		break;
	case VLIW_CSRRC:
		csr_insn(CSR_TYPE_C, vlp->imm&0xfff, vlp->rd, vlp->rs1, (uint32_t)reg[vlp->rs1]);
		break;
	case VLIW_CSRRWI:
		csr_insn(CSR_TYPE_W, vlp->imm&0xfff, vlp->rd, vlp->rs1, vlp->rs1);
		break;
	case VLIW_CSRRSI:
		csr_insn(CSR_TYPE_S, vlp->imm&0xfff, vlp->rd, vlp->rs1, vlp->rs1);
		break;
	case VLIW_CSRRCI:
		csr_insn(CSR_TYPE_C, vlp->imm&0xfff, vlp->rd, vlp->rs1, vlp->rs1);
		break;


		//******************************************************
		//		RV64
		//******************************************************
		// RV64 specific instructions
#if RV64
	// 32 bit add
	case VLIW_ADDIW:
		reg[vlp->rd] = (int32_t)(reg[vlp->rs1] + vlp->imm);
		break;

	case VLIW_ADDW:
		reg[vlp->rd] = (uint32_t)(reg[vlp->rs1] + reg[vlp->rs2]);
		break;

	case VLIW_SUBW:
		reg[vlp->rd] = (uint32_t)(reg[vlp->rs1] - reg[vlp->rs2]);
		break;

	case VLIW_LWU:
		addr32 = reg[vlp->rs1] + vlp->imm;
		reg[vlp->rd] = (uint32_t)memory->read32(addr32);
		if(0 || cpu->verbose) {
			printf("lwu: 0x%08x => 0x%08x (r%d)\n", addr32, (uint32_t)reg[vlp->rd], vlp->rd);
		}
		break;

	case VLIW_LD:
		addr32 = reg[vlp->rs1] + vlp->imm;
		reg[vlp->rd] = memory->read64(addr32);
		if(0 || cpu->verbose) {
			printf("ld: 0x%08x => 0x%016llx (r%d)\n", addr32, reg[vlp->rd], vlp->rd);
		}
		break;

	case VLIW_SD:
		addr32 = reg[vlp->rs1] + vlp->imm;
		memory->write64(addr32, (uint64_t)reg[vlp->rs2]);
		if(0 || cpu->verbose) {
			printf("sw: 0x%08x <= 0x%016llx (r%d)\n", addr32, (uint64_t)reg[vlp->rs2], vlp->rs2);
		}
		break;
#endif

	default:
		vlp->dump();
		fatal("Execute: Unknown op = 0x%02x\n", vlp->opcode);
		break;
	}
	// R0 is always Zero
	reg[0] = 0;		// just in case it was written, then stomped it!



#ifdef DO_TRACE
	// Limit Execution
	static int cnt = 0;
	if(cnt++ >= 100000) {
		printf("Trace ... full\n");
		exit(0);
	}

	// Trace File
	if(tfp == NULL) {
		tfp = fopen("trace.txt", "w");
	}
	if(tfp) {
		const char* rd = getRegName(vlp->rd & 0x1f);
		const char* rs1 = getRegName(vlp->rs1 & 0x1f);
		const char* rs2 = getRegName(vlp->rs2 & 0x1f);
		uint32_t addr32 = (uint32_t)reg[vlp->rs1] + vlp->imm;


		fprintf(tfp, "%08x: %s %s,%s,%s, %08x\n", (uint32_t)pc, VliwName[vlp->opcode],
			rd, rs1, rs2, vlp->imm);

		switch(vlp->opcode) {
		case VLIW_SW:
			fprintf(tfp, "  sw %s,%d[%s]\n", rs2, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x <= 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rs2], memory->read32(addr32));
			break;

		case VLIW_SH:
			fprintf(tfp, "  sh %s,%d[%s]\n", rs2, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x <= 0x%08x [%04x]\n",
				addr32, (uint32_t)reg[vlp->rs2], memory->read16(addr32));
			break;

		case VLIW_SB:
			fprintf(tfp, "  sb %s,%d[%s]\n", rs2, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x <= 0x%08x [%02x]\n",
				addr32, (uint32_t)reg[vlp->rs2], memory->read8(addr32));
			break;

		case VLIW_LW:
			fprintf(tfp, "  lw %s,%d[%s]\n", rd, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x => 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rd], memory->read32(addr32));
			break;

		case VLIW_LH:
			fprintf(tfp, "  lh %s,%d[%s]\n", rd, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x => 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rd], memory->read32(addr32));
			break;

		case VLIW_LHU:
			fprintf(tfp, "  lhu %s,%d[%s]\n", rd, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x => 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rd], memory->read32(addr32));
			break;

		case VLIW_LB:
			fprintf(tfp, "  lb %s,%d[%s]\n", rd, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x => 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rd], memory->read32(addr32));
			checkAddr(addr32);
			break;

		case VLIW_LBU:
			fprintf(tfp, "  lbu %s,%d[%s]\n", rd, vlp->imm, rs1);
			fprintf(tfp, "  0x%08x => 0x%08x [%08x]\n",
				addr32, (uint32_t)reg[vlp->rd], memory->read32(addr32));
			checkAddr(addr32);
			break;

		case VLIW_JAL:
			if(vlp->rd == 1) {
				fprintf(tfp, "  Call: %08x\n", pc_next);
			}
			break;

		case VLIW_BEQ:
			fprintf(tfp, "  beq %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x == 0x%08x - %d\n",
				(uint32_t)reg[vlp->rs1], (uint32_t)reg[vlp->rs2],
				(uint32_t)reg[vlp->rs1] == (uint32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_BNE:
			fprintf(tfp, "  bne %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x != 0x%08x - %d\n",
				(uint32_t)reg[vlp->rs1], (uint32_t)reg[vlp->rs2],
				(uint32_t)reg[vlp->rs1] != (uint32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_BGE:
			fprintf(tfp, "  bge %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x >= 0x%08x - %d\n",
				(int32_t)reg[vlp->rs1], (int32_t)reg[vlp->rs2],
				(int32_t)reg[vlp->rs1] >= (int32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_BLT:
			fprintf(tfp, "  blt %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x < 0x%08x - %d\n",
				(int32_t)reg[vlp->rs1], (int32_t)reg[vlp->rs2],
				(int32_t)reg[vlp->rs1] < (int32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_BGEU:
			fprintf(tfp, "  bgeu %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x >= 0x%08x - %d\n",
				(uint32_t)reg[vlp->rs1], (uint32_t)reg[vlp->rs2],
				(uint32_t)reg[vlp->rs1] >= (uint32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_BLTU:
			fprintf(tfp, "  bltu %s,%s,%08x\n", rs1, rs2, pc+vlp->imm);
			fprintf(tfp, "  0x%08x < 0x%08x - %d\n",
				(uint32_t)reg[vlp->rs1], (uint32_t)reg[vlp->rs2],
				(uint32_t)reg[vlp->rs1] < (uint32_t)reg[vlp->rs2]);
			if(pc_next != pc_inc) {
				fprintf(tfp, "  Bra/Jmp: %08x\n", pc_next);
			}
			break;

		case VLIW_ADDI:
			fprintf(tfp, "  addi %s,%s,%d\n", rd, rs1, vlp->imm);
			fprintf(tfp, "  %s <= 0x%08x\n", rd, (uint32_t)reg[vlp->rd]);
			break;

		case VLIW_ADD:
			fprintf(tfp, "  addi %s,%s,%s\n", rd, rs1, rs2);
			fprintf(tfp, "  %s <= 0x%08x\n", rd, (uint32_t)reg[vlp->rd]);
			break;

		default:
			if(vlp->rd != 0) {
				fprintf(tfp, "  %s = %08x\n", rd, (uint32_t)reg[vlp->rd]);
			}
			break;
		}

	}
#endif


	return true;
};


int64_t time_us(void) {
	//	struct timeval {
	//		time_t			tv_sec;		// int64_t  - seconds
	//		suseconds_t		tv_usec;    // uint32_t - microseconds
	//	};
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t value = (uint64_t)tv.tv_sec*1000000;	// uint64_t - seconds
	value += (uint32_t)tv.tv_usec;					// microseconds
	return value;
};

//=====================================================================
//		csr_insn - Control Status Registers
//=====================================================================
//====================================================================
// 		CSR Unit
//=============================================================================
//
//		001 - CSRRW		Atomic Read/Write CSR, Register
//		010 - CSRRS		Atomic Read and Set Bits in CSR, Register
//		011 - CSRRC		Atomic Read and Clear Bits in CSR, Register
//		101 - CSRRWI	Atomic Read/Write CSR, Immediate
//		110 - CSRRSI	Atomic Read and Set Bits in CSR, Immediate
//		111 - CSRRCI	Atomic Read and Clear Bits in CSR, Immediate
//
//		Register operand
//		----------------------------------------------
//		Instruction		rd 		rs1		Read	Write
//		----------------------------------------------
//		CSRRW			x0		-		no		yes
//		CSRRW			!x0		-		yes		yes
//		CSRRS/C			-		x0		yes		no
//		CSRRS/C			-		!x0		yes		yes
//
//		Immediate operand
//		----------------------------------------------
//		Instruction		rd 		uimm	Read	Write
//		----------------------------------------------
//		CSRRWI			x0		-		no		yes
//		CSRRWI			!x0		-		yes		yes
//		CSRRS/CI		-		0		yes		no
//		CSRRS/CI		-		!0		yes		yes
//====================================================================
void Cpu::csr_insn(int type, int csr, int rd, int rs, uint32_t imm32) {
	if(verbose) {
		printf("Control reg: 0x%03x, rd=%d, imm32=%08x\n", csr, rd, imm32);
	}

	bool csr_read = (type==CSR_TYPE_S || type==CSR_TYPE_C) | (rd!=0);
	bool csr_write = type==CSR_TYPE_W || ((type==CSR_TYPE_S || type==CSR_TYPE_C) & (rs!=0));

	// CSR Registers

	uint32_t value = 0;
	uint64_t temp64;

	// Read the CSR
	if(csr_read)
		switch(csr) {
		// Machine Info
		case RISCV_CSR_MVENDORID:	// mvendorid	Machine Vendor ID
			value = OPT_MVENDOR_ID;
			break;
		case RISCV_CSR_MARCHID:		// marchid		Machine Architecture ID
			value = OPT_MARCH_ID;
			break;
		case RISCV_CSR_MIMPID:		// mimpid		Machine Implementation ID
			value = OPT_MIMP_ID;
			break;
		case RISCV_CSR_MHARTID: 	// mhartid		Machine Hardware Thread ID
			value = OPT_MHART_ID;
			break;

		// Machine Trap Setup
		case RISCV_CSR_MSTATUS:		// mstatus		Machine Status
			value = mstatus;
			break;
		case RISCV_CSR_MISA: 		// misa			Machine ISA and Extensions
			value = OPT_MISA;
			break;
		case RISCV_CSR_MIE: 		// mie			Machine Interrupt Enable
			value = mie;
			break;
		case RISCV_CSR_MTVEC: 		// mtvec		Machine Trap Handler
			value = mtvec;
			break;

		// Machine Trap Handling
		case RISCV_CSR_MSCRATCH: 	// mscratch		Machine Scratch Register
			value = mscratch;
			break;
		case RISCV_CSR_MEPC: 		// mepc			Machine Exception Program Counter
			value = mepc;
			break;
		case RISCV_CSR_MCAUSE: 		// mcause		Machine Exception Cause
			value = mcause;
			break;
		case RISCV_CSR_MTVAL: 		// mtval		Machine Trap Value
			value = mtval;
			break;
		case RISCV_CSR_MIP: 		// mip			Machine Interrupt Pending
			value = mip;
			break;

		// Machine Counter/Timers
		//	Just use cpu::cycle for all
		case RISCV_CSR_CYCLE:			// cycle		Cycle counter for RDCYCLE instruction
		case RISCV_CSR_MCYCLE: 			// mcycle		Clock Cycles Executed Counter
			value = (uint32_t)cycle;
			break;
		case RISCV_CSR_CYCLEH:			// cycleh		Upper 32 bits of cycle, RV32I only
		case RISCV_CSR_MCYCLEH: 		// mcycleh		Upper 32 bits of mcycle, RV32I only
			value = (uint32_t)(cycle>>32);
			break;

		// TIME/TIMEH - very roughly about 1 ms per tick
		case RISCV_CSR_TIME:			// time			Timer for RDTIME instruction
			temp64 = time_us();
			value = (uint32_t)temp64;
			break;
		case RISCV_CSR_TIMEH:			// timeh		Upper 32 bits of time, RV32I only
			temp64 = time_us();
			value = (uint32_t)(temp64>>32);
			break;

		// Number of Instructions Retired Counter, just use cycle
		case RISCV_CSR_INSTRET: 		// instret		Number of Instructions Retired Counter
		case RISCV_CSR_MINSTRET: 		// minstret		Number of Instructions Retired Counter
			value = (uint32_t)cycle;
			break;
		case RISCV_CSR_INSTRETH: 		// instreth		Upper 32 bits of minstret, RV32I only
		case RISCV_CSR_MINSTRETH: 		// minstreth	Upper 32 bits of minstret, RV32I only
			value = (uint32_t)(cycle>>32);
			break;

		// Custom CSRs
		case RISCV_CSR_RAND:
			rand32(&value);	// assume always ok
			break;

		default:
			value = 0;
			break;
		}

	// SET bits
	uint32_t new_val = imm32;
	if(type == CSR_TYPE_S) {
		new_val = value | imm32;
	}

	// CLEAR bits
	else if(type == CSR_TYPE_C) {
		new_val = value & ~imm32;
		// printf("CSR Clear: %03x, %08x (%08x)\n", csr, new_val, imm32);
	}

	// Write the CSR
	if(csr_write)
		switch(csr) {

		case RISCV_CSR_MTVEC: 		// mtvec		Machine Trap Handler
			mtvec = new_val;
			break;
		case RISCV_CSR_MEPC: 		// mepc			Machine Exception Program Counter
			mepc = new_val;
			break;
		case RISCV_CSR_MSCRATCH:	// mscratch		Machine Scratch Register
			mscratch = new_val;
			break;

		case RISCV_CSR_MIE: 		// mie			Machine Interrupt Enable
			mie = new_val;
			break;

		case RISCV_CSR_MIP: 		// mip			Machine Interrupt Pending
			mip = new_val;
			break;

		case RISCV_CSR_MSTATUS:		// mstatus		Machine Status
			mstatus = new_val;
			break;

		case RISCV_CSR_MCYCLE: 		// mcycle		Clock Cycles Executed Counter
			// should set upper half first ...			
			cycle = new_val;
			break;

		case RISCV_CSR_MCYCLEH:		// mcycleh		Upper 32 bits of cycle, RV32I only
			// note lower half is set to zero ...			
			cycle = (((uint64_t)new_val)<<32);
		default:
			break;
		}
	// printf("mip = %08x, mie = %08x\n", mip, mie);

	if(rd != 0) {
		reg[rd] = value;
	}

#if 0
	if(csr == RISCV_CSR_MSTATUS) {
		printf("Control reg: 0x%03x, rd=%d, imm32=%08x\n", csr, rd, imm32);
		printf("mstatus = %08x, new_val = %08x, csr_read = %d, csr_write = %d\n",
			value, new_val, csr_read, csr_write);
	}
#endif

};


//=====================================================================
//		Ecall/Ebreak/Mret Exception Processing
//=====================================================================
void Cpu::ecall(void) {
	// save the current PC, EPC is this instruction!!!
	mepc = pc;
	mcause = RISCV_EXCP_ECALL_U;	// mcause[31] = 0, exception
	// vector off to trap handler
	pc_next = mtvec;

	if((mstatus&RISCV_MSTATUS_MIE)==0) {
		mstatus &= ~RISCV_MSTATUS_MPIE;    // clear mpie
	}
	else {
		mstatus |= RISCV_MSTATUS_MPIE;    // set mpie
	}
};

void Cpu::ebreak(void) {
	// save the current PC, EPC is this instruction!!!
	mepc = pc;
	mcause = RISCV_EXCP_BREAK;	// mcause[31] = 0, exception
	// vector off to trap handler
	pc_next = mtvec;
	if((mstatus&RISCV_MSTATUS_MIE)==0) {
		mstatus &= ~RISCV_MSTATUS_MPIE;    // clear mpie
	}
	else {
		mstatus |= RISCV_MSTATUS_MPIE;    // set mpie
	}
};

void Cpu::mret(void) {
	// restore the previous PC
	pc_next = mepc;
	if((mstatus&RISCV_MSTATUS_MPIE)==0) {
		mstatus &= ~RISCV_MSTATUS_MIE;    // clear mie
	}
	else {
		mstatus |= RISCV_MSTATUS_MIE;    // set mie
	}
	mstatus &= ~RISCV_MSTATUS_MPIE;	// clear mpie, not needed, just reduce state leaking
};

bool Cpu::do_external_irq(void) {
	// Machine External Interrupt
	if((mip&RISCV_MIP_MEIP)==0 || (mie&RISCV_MIE_MEIE)==0) {
		return false;
	}
	if((mstatus&RISCV_MSTATUS_MIE)==0) {
		return false;
	}

	mstatus |= RISCV_MSTATUS_MPIE;	// set previous enabled
	mstatus &= ~RISCV_MSTATUS_MIE;	// clear enable bit
	mepc = pc;
	mcause = 0x80000000 | RISCV_INTR_MEI;	// mcause[31] = 1, interrupt
	// vector off to trap handler
	pc_next = mtvec;
	return true;
};

bool Cpu::do_time_irq(void) {
	// Machine External Interrupt
	// if((mip&RISCV_MIP_MTIP)==0 || (mie&RISCV_MIE_MTIE)==0) return false;
	if((mip&mie&RISCV_MIE_MTIE)==0) {
		return false;
	}
	if((mstatus&RISCV_MSTATUS_MIE)==0) {
		return false;
	}

	mstatus |= RISCV_MSTATUS_MPIE;	// set previous enabled
	mstatus &= ~RISCV_MSTATUS_MIE;	// clear enable bit
	mepc = pc;
	mcause = 0x80000000 | RISCV_INTR_MTI;	// mcause[31] = 1, interrupt
	// printf("**** do_time_irq ***** - mcause = %08x\n", mcause);
	// vector off to trap handler
	pc_next = mtvec;
	return true;
};





//=====================================================================
//		Newlib System Call
//=====================================================================
typedef struct {
	int			num;
	const char* name;
} SyscallName;

SyscallName syscallName[] {
	{ SYS_exit,			"SYS_exit" },
	{ SYS_write,		"SYS_write" },
	{ SYS_read,			"SYS_read" },
	{ SYS_close,		"SYS_close" },
	{ SYS_open,			"SYS_open" },
	{ SYS_fstat,		"SYS_fstat" },
	{ SYS_brk,			"SYS_brk" },
	{ SYS_gettimeofday,	"SYS_gettimeofday" },
	{ SYS_time,			"SYS_time" },
	{ 0, NULL }
};

const char* getSyscallName(int n) {
	for(SyscallName* snp = syscallName; snp->name != NULL; ++snp) {
		if(snp->num == n) {
			return snp->name;
		}
	}
	return "???";
};



// sys_brk - sys_brk(prev, inc)
//	Notes:
//		- 8 byte alignment for malloc chunks
//		- 4k aligned for sbrk blocks
//		- parameters:
//			a0 - previous heap, 0 = reset to end of data
//			a1 - increment size, ignored if a0 == 0
//		- 4k align end of data (or 8/16 byte align???)
uint32_t Cpu::sys_brk(uint32_t a0, uint32_t a1) {
	// uint32_t page_size = 0x1000;	// 4K page size
	uint32_t page_size = 0x10;		// 16 byte alignment
	verbose = 0;

	if(verbose) {
		printf("  brk(%08x, %08x)\n", a0, a1);
	}
	uint32_t prev_heap = a0;
	if(a0 == 0) {
		// set up new heap
		heap = endOfData;
		// align to 4k boundary
		heap = ((uint32_t)endOfData + (page_size-1)) & ~(page_size-1);
		// printf("  Heap was zero: %08x\n", (uint32_t)heap);
		prev_heap = heap;
	}
	else {
		heap = a0 + a1;	// set new value
	}
	if(verbose) {
		printf("  Heap: Old = %08x, New = %08x\n", prev_heap, heap);
	}

	return (uint32_t)prev_heap;	// return 0;
}


// int gettimeofday(struct timeval *tv, struct timezone *tz );
uint32_t Cpu::sys_gettimeofday(uint32_t a0) {
	// struct timeval {
	//	time_t      tv_sec;     /* seconds */
	//    suseconds_t tv_usec;    /* microseconds */
	// };
	// printf("Ptr to: %08x\n", a0);
	// fflush(stdout);


	uint32_t* ptr = (uint32_t*)memory->getPtr(a0);
#if 0
	ptr[0] = time(NULL);
	ptr[1] = 0;
#else

	//	struct timeval {
	//		time_t			tv_sec;		// uint64_t - seconds
	//		suseconds_t		tv_usec;    // uint32_t - microseconds
	//	};
	struct timeval tv;
	// printf("Sizeof(time_t) = %d\n", sizeof(time_t));
	// printf("Sizeof(suseconds_t) = %d\n", sizeof(suseconds_t));
	// printf("offset(tv_usec) = %d\n", (char*)(&(tv.tv_usec)) - (char*)(&(tv.tv_sec)));

	gettimeofday(&tv, NULL);
	int64_t temp64 = tv.tv_sec;		// could be 32 or 64 bits
	ptr[0] = (uint32_t)temp64;	// lsb Seconds
	ptr[1] = (uint32_t)((temp64)>>32);	// msb Seconds
	ptr[2] = (uint32_t)tv.tv_usec;			// microseconds
#endif
	return(0);	// success
}

/*
int Cpu::sys_fstat(int file, struct stat *st) {
  st->st_mode = S_IFCHR;
  return 0;
}

int32_t Cpu::sys_close(int fd) {
  return -1;
}

int Cpu::sys_lseek(int file, int offset, int whence) {
  return 0;
}
*/

//	int _read (int fd, char *buf, int count);
int Cpu::sys_read(uint32_t a0, uint32_t a1, uint32_t a2) {
	int fd  = a0;
	uint8_t* buf = memory->getPtr(a1);
	int count = a2;
	int read = 0;

	// stdin
	if(fd == 0) {
		for(; count > 0; --count) {
			int c = fgetc(stdin);
			if(c == EOF) {
				break;
			}
			*buf++ = c;
			read++;
			if(c == '\n') {
				break;
			}
		}
	}
	else {
		return 0;
	}

	return read;
}

// int _write (int fd, char *buf, int count)
int Cpu::sys_write(uint32_t a0, uint32_t a1, uint32_t a2) {
	int fd  = a0;
	uint8_t* buf = memory->getPtr(a1);
	int count = a2;
	int n = 0;

	// stdin
	if(fd == 1) {
		for(; count > 0; --count) {
			int c = *buf++;
			// if (c == EOF) break;
			fputc(c, stdout);
			n++;
		}
	}
	// stderr
	else if(fd == 2) {
		for(; count > 0; --count) {
			int c = *buf++;
			// if (c == EOF) break;
			fputc(c, stderr);
			n++;
		}
	}
	else {
		return 0;
	}

	return n;
}



int32_t Cpu::sys_open(uint32_t pathname, uint32_t flags, uint32_t mode) {
	printf("sys_open: pathname=0x%08x, flags=0x%08x, mode=0x%08x\n", pathname, flags, mode);
	return -1;	// Error
}

int32_t Cpu::sys_close(uint32_t fd) {
	(void)fd;
	printf("sys_close: fd=0x%08x\n", fd);
	return -1;	// Error
}

// See:
//	https://interrupt.memfault.com/blog/boostrapping-libc-with-newlib
//
void Cpu::syscall(void) {
	uint32_t a0 = (uint32_t)reg[10];		// a0, 1st parameter
	uint32_t a1 = (uint32_t)reg[11];		// a1, 2nd parameter ...
	uint32_t a2 = (uint32_t)reg[12];		// a2
	uint32_t a3 = (uint32_t)reg[13];		// a3
	uint32_t a4 = (uint32_t)reg[14];		// a4
	uint32_t a5 = (uint32_t)reg[15];		// a5
	uint32_t a6 = (uint32_t)reg[16];		// a6
	uint32_t a7 = (uint32_t)reg[17];		// a7, system call #

	int verbose = 0;

	if(verbose) {
		printf("Ecall: %s (%d - 0x%02x)\n", getSyscallName(a7), a7, a7);
		printf("  a0=0x%08x, a1=0x%08x, a2=0x%08x, a3=0x%08x\n", a0, a1, a2, a3);
		printf("  a4=0x%08x, a5=0x%08x, a6=0x%08x, a7=0x%08x\n", a4, a5, a6, a7);
	}

	switch(a7) {
	case SYS_brk:
		reg[10] = sys_brk(a0, a1);
		return;

	case SYS_gettimeofday:
		reg[10] = sys_gettimeofday(a0);
		return;

	case SYS_exit:
		printf("  Exit ....\n");
		printf("    Cycles = %s\n", dec64(cycle));
		if(a0 == 0) {
			printf("    Normal Exit (%d)\n", a0);
		}
		else {
			printf("    Exit Code = %d\n", a0);
		}
		exit(0);

	case SYS_read:
		reg[10] = sys_read(a0, a1, a2);
		return;

	case SYS_write:
		reg[10] = sys_write(a0, a1, a2);
		return;

	case SYS_open:
		reg[10] = sys_open(a0, a1, a2);
		return;

	case SYS_close:
		reg[10] = sys_close(a0);
		return;

	case SYS_fstat:
		reg[10] = 0;	// return 0;
		return;

	case SYS_time:
		reg[10] = time(NULL);
		printf("  Time=0x%08x\n", (uint32_t)reg[10]);
		return;

	default:
		printf("Ecall: %s (%d - 0x%02x)\n", getSyscallName(a7), a7, a7);
		printf("  a0=0x%08x, a1=0x%08x, a2=0x%08x, a3=0x%08x\n", a0, a1, a2, a3);
		printf("  a4=0x%08x, a5=0x%08x, a6=0x%08x, a7=0x%08x\n", a4, a5, a6, a7);
		printf("Ecall - not handled\n");
		exit(-1);
	}
};



#if 0
// loadStore() - load/store unit
//	rw - 0 = read, 1 = write, -1 = fetch
unsigned Cpu::loadStore(unsigned addr, int rw, int size, unsigned value) {
	unsigned ret = 0xdeadbeef;

	// debug Uart
	if((addr&0xffffff00) == 0xfffffe00) {
		if((addr&0xff)==0x00 && rw == 1) {
			int c = value & 0xff;
#if 0
			if(verbose) {
				if(isprint(c)) {
					printf("Uart: %c\n", c);
				}
				else {
					printf("Uart: %02x\n", c);
				}
			}
			else {
				printf("%c", c);
			}
#else
			static char buf[256];
			static int index = 0;

			if(c != '\n') {
				buf[index++] = c;
			}
			buf[index] = '\0';
			if(c == '\n' || index >= 256-1) {
				index = 0;
				// Form1->RichEdit2->Lines->Add(buf);
				printf("Uart: %s\n", buf);
			}
#endif

		}
		return 0x00000000;	// default return values to zero (ready to transmit)
	}

	// debug Timer
	if((addr&0xffffff00) == 0xfffffc00) {
		unsigned reg = (addr&0x3f)>>2;
		if(reg == 0 && rw == 0) {
			Sleep(500);		// *** fake a simple timer !!!!
			return 0x80000000;
		}

		return 0x00000000;	// default return values to zero (ready to transmit)
	}

	// HpeMini Leds
	if((addr&0xffffff00) == 0xfffffd00) {
		unsigned reg = (addr&0x3f)>>2;
		if(reg == 0 && rw == 0) {
			return 0x8000000;
		}

		return 0x00000000;	// default return values to zero (ready to transmit)
	}


	if(!quiet && /*verbose && */ rw == 1) {
		if(size == 4) {
			printf("Store: %08x <= %08x\n", addr, value);
		}
		else if(size == 2) {
			printf("Store: %08x <= %04x\n", addr, value&0xffff);
		}
		else if(size == 1) {
			char c = value;
			printf("Store: %08x <= %02x (%c)\n", addr, c, (isprint(c)?c:'?'));
		}
		else {
			printf("Store: Bad size = %d\n", size);
		}
	}
	if(lfp && rw == 1) {
		if(size == 4) {
			fprintf(lfp, "Store: %08x <= %08x\n", addr, value);
		}
		else if(size == 2) {
			fprintf(lfp, "Store: %08x <= %04x\n", addr, value&0xffff);
		}
		else if(size == 1) {
			char c = value;
			fprintf(lfp, "Store: %08x <= %02x (%c)\n", addr, c, (isprint(c)?c:'?'));
		}
		else {
			fprintf(lfp, "Store: Bad size = %d\n", size);
		}
	}

	if(addr == 0x0000ffff && rw == 1) {	// pseudo uart
		char c = value;
		printf("Uart: %02x (%c)\n", c, (isprint(c)?c:'?'));
		// uartSend(c);
	}

	unsigned char* mem;
	unsigned offset;
	if(addr >= insnMem->memBase &&  addr < (insnMem->memBase+insnMem->memLen)) {
		mem = insnMem->data;
		offset = addr - insnMem->memBase;
	}
	else if(addr >= dataMem->memBase && addr < (dataMem->memBase+dataMem->memLen)) {
		mem = dataMem->data;
		offset = addr - dataMem->memBase;
	}
	else {
		printf("Invalid address; %08x\n", addr);
		if(lfp) {
			fprintf(lfp, "Invalid address; %08x\n", addr);
		}
		return 0x00000000;
	}

	{
		// memory access
		if(rw <= 0) {	// read or fetch
			if(size == 4) {
				if(endian == 0) {	// big endian
					ret = ((mem[offset]<<24) | (mem[offset+1]<<16) | (mem[offset+2]<<8) | mem[offset+3]);
				}
				else {	// little endian
					ret = ((mem[offset+3]<<24) | (mem[offset+2]<<16) | (mem[offset+1]<<8) | mem[offset+0]);
				}
			}
			else if(size == 2) {
				if(endian == 0) {	// big endian
					ret = ((mem[offset+0]<8) | mem[offset+1]);
				}
				else {	// little endian
					ret = ((mem[offset+1]<8) | mem[offset+0]);
				}
			}
			else if(size == 1) {
				ret = mem[offset];
			}
		}
		else {	// write
			if(size == 4) {
				if(endian == 0) {	// big endian
					mem[offset+0] = value>>24;
					mem[offset+1] = value>>16;
					mem[offset+2] = value>>8;
					mem[offset+3] = value;
				}
				else {	// little endian
					mem[offset+3] = value>>24;
					mem[offset+2] = value>>16;
					mem[offset+1] = value>>8;
					mem[offset+0] = value;
				}
			}
			if(size == 2) {
				if(endian == 0) {	// big endian
					mem[offset+0] = value>>8;
					mem[offset+1] = value;
				}
				else {	// little endian
					mem[offset+1] = value>>8;
					mem[offset+0] = value;
				}
			}
			if(size == 1) {
				mem[offset] = value;
			}
		}
	}

	if(!quiet && /* verbose && */ rw == 0) {
		printf("Load: %08x => %08x\n", offset, ret);
	}
	if(lfp && rw == 0) {
		fprintf(lfp, "Load: %08x => %08x\n", offset, ret);
	}
	return ret;
};
#endif


#ifdef __x86_64__	// X86_RAND
// x86 access
#include <immintrin.h>
#include <cpuid.h>
bool cpu_has_aes = false;		// does the CPU have AES instructions?
bool cpu_has_rand = false;		// does the CPU have RAND instructions?

//=====================================================================
//		x86 Random Generator, CSR address 0x015
//=====================================================================
#define RDRAND_AES	0x02000000
#define RDRAND_MASK	0x40000000
#define X86_PSN		(1<<18)
int cpuid(void) {
	int eax, ebx, ecx, edx;
	char vendor[13];

	// "GenuineIntel"
	__cpuid(0, eax, ebx, ecx, edx);
	memcpy(vendor, &ebx, 4);
	memcpy(vendor + 4, &edx, 4);
	memcpy(vendor + 8, &ecx, 4);
	vendor[12] = '\0';
	printf("CPU: %s\n", vendor);


	// Info/Feature Bits
	__cpuid(1, eax, ebx, ecx, edx);
	printf("Info/Feature Bits:\n");
	printf("  eax: %08x\n", eax);
	printf("  ebx: %08x\n", ebx);
	printf("  ecx: %08x\n", ecx);
	printf("  edx: %08x\n", edx);


	if(ecx&RDRAND_AES) {
		printf("Has AES!\n");
		// int ret = aes128_self_test();
		// printf("  AES = %s (%d)\n", (ret==0)?"OK":"Bad!", ret);
		cpu_has_aes = true;
	}

	if((ecx&RDRAND_MASK)==0) {
		printf("Does not have RDRAND!\n");
		return 0;
	}
	else {
		printf("Has RDRAND!\n");
		cpu_has_rand = true;
	}


	// Processor Serial Number
	if(edx&X86_PSN) {
		__cpuid(3, eax, ebx, ecx, edx);
		printf("Processor Serial Number:\n");
		printf("  eax: %08x\n", eax);
		printf("  ebx: %08x\n", ebx);
		printf("  ecx: %08x\n", ecx);
		printf("  edx: %08x\n", edx);
	}

#if 0
	uint32_t ret = __get_cpuid_max();
	printf("get_cpuid_max: %08x\n", ret);
	__cpuid(16, eax, ebx, ecx, edx);
	printf("CPU Frequency:\n");
	printf("  eax: %08x\n", eax);
	printf("  ebx: %08x\n", ebx);
	printf("  ecx: %08x\n", ecx);
	printf("  edx: %08x\n", edx);
#endif

	return 1;
}

//=============================================================================
//		X86 Random HW
//=============================================================================
// These intrinsics generate random numbers of 16/32/64 bit wide random integers.
// The generated random value is written to the given memory location and the success
// status is returned: '1' if the hardware returned a valid random value, and '0' otherwise.
//	xorl    %eax, %eax
//	rdrand  %rax
//	movl    $1, %edx
//	...
//	cmovc   %edx, %eax
int rand64(uint64_t* value) {
	unsigned long long result = 0ULL;
	int rc = _rdrand64_step(&result);
	bool ok = (rc == 1);
	if(ok) {
		*value = (uint64_t)result;
	}
	return !ok;		// 0 = ok, !0 = bad
}

int rand32(uint32_t* value) {
	int rc = _rdrand32_step(value);
	// printf("Rand: value = %d, rc = %d\n", value, rc);
	bool ok = (rc == 1);
	return !ok;		// 0 = ok, !0 = bad
}

int rand16(uint16_t* value) {
	int rc = _rdrand16_step(value);
	// printf("Rand: value = %d, rc = %d\n", value, rc);
	bool ok = (rc == 1);
	return !ok;		// 0 = ok, !0 = bad
}
#else 		// !X86_RAND
int rand32(uint32_t* value) {
	*value = (uint32_t)rand();
	return 0;
}
#endif		// X86_RAND

