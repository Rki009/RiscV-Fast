//***************************************************************
//	Compressed decoding
//***************************************************************
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>

#include "RiscVFast.h"
#include "Cpu.h"


Vliw* fastTable[0x10000];		// 64K fast VLIW access table

// map 3-bit register to 5 bit value
int regMap3[8] = { 8, 9, 10, 11, 12, 13, 14, 15 };
int fregMap3[8] = { 8, 9, 10, 11, 12, 13, 14, 15 };

int32_t mk_imm6(uint32_t code) {
	int32_t b5 = (code>>12)&0x1;
	int32_t b4_0 = (code>>2)&0x1f;
	int32_t imm = (b5<<5) | (b4_0<<0);
	if((imm&0x00000020) != 0) {
		imm |= 0xffffffc0;
	}
	return imm;
};

int32_t jal_off(uint16_t code) {
	int32_t b5 = (code>>2)&0x1;
	int32_t b3_1 = (code>>3)&0x7;
	int32_t b7 = (code>>6)&0x1;
	int32_t b6 = (code>>7)&0x1;
	int32_t b10 = (code>>8)&0x1;
	int32_t b9_8 = (code>>9)&0x3;
	int32_t b4 = (code>>11)&0x1;
	int32_t b11 = (code>>12)&0x1;

	int32_t imm = (b11<<11) | (b10<<10) | (b9_8<<8) | (b7<<7) | (b6<<6)
		| (b5<<5) | (b4<<4) | (b3_1<<1);
	if(b11 != 0) {
		imm |= 0xfffff000;
	}
	return imm;
};

// C_LWSP - zero extended (6 bits << 2)
int32_t mk_lwsp(uint16_t code) {
	int32_t b5 = (code>>12)&0x1;
	int32_t b4_2 = (code>>4)&0x7;
	int32_t b7_6 = (code>>2)&0x3;
	int32_t imm = (b7_6<<6)| (b5<<5) | (b4_2<<2);
	// if ((imm&0x000080) != 0) imm |= 0xffffff00;
	return imm;
};

int32_t mk_addi(uint16_t code) {
	int32_t b5 = (code>>12)&0x1;
	int32_t b4_0 = (code>>2)&0x1f;
	int32_t imm = (b5<<5) | (b4_0<<0);
	if((imm&0x00000020) != 0) {
		imm |= 0xffffffc0;
	}
	return imm;
};

//	shamt (6 bit unsigned)
int32_t mk_shamt(uint16_t code) {
	int32_t b5 = (code>>12)&0x1;
	int32_t b4_0 = (code>>2)&0x1f;
	int32_t imm = (b5<<5) | (b4_0<<0);
	// if ((imm&0x00000020) != 0) imm |= 0xffffffc0;
	return imm;
};

int32_t mk_addi16sp(uint16_t code) {
	int32_t b9 = (code>>12)&0x1;
	int32_t b4 = (code>>6)&0x1;
	int32_t b6 = (code>>5)&0x1;
	int32_t b8_7 = (code>>3)&0x3;
	int32_t b5 = (code>>2)&0x1;
	int32_t imm = (b9<<9) | (b8_7<<7) | (b6<<6) | (b5<<5) | (b4<<4);
	if((imm&0x00000200) != 0) {
		imm |= 0xfffffc00;
	}
	return imm;
};

//	nzuimm[5:4|9:6|2|3]
int32_t mk_addi4spn(uint16_t code) {
	int32_t b5_4 = (code>>11)&0x3;
	int32_t b9_6 = (code>>7)&0xf;
	int32_t b2 = (code>>6)&0x1;
	int32_t b3 = (code>>5)&0x1;
	int32_t imm = (b9_6<<6) | (b5_4<<4) | (b3<<3) | (b2<<2);
	return imm;
};

int32_t mk_beq(uint16_t code) {
	int32_t b8 = (code>>12)&0x1;
	int32_t b4_3 = (code>>10)&0x3;
	int32_t b7_6 = (code>>5)&0x3;
	int32_t b2_1 = (code>>3)&0x3;
	int32_t b5 = (code>>2)&0x1;
	int32_t imm = (b8<<8) | (b7_6<<6) | (b5<<5) | (b4_3<<3) | (b2_1<<1);
	if(b8) {
		imm |= 0xfffffe00;
	}
	return imm;
};

int32_t mk_sw(uint16_t code) {
	int32_t b5_3 = (code>>10)&0x7;
	int32_t b2 = (code>>6)&0x1;
	int32_t b6 = (code>>5)&0x1;
	int32_t imm = (b6<<6) | (b5_3<<3) | (b2<<2);
	if(b6) {
		imm |= 0xffffffc0;
	}
	return imm;
};

int32_t mk_sd(uint16_t code) {
	int32_t b5_3 = (code>>10)&0x7;
	int32_t b7_6 = (code>>5)&0x3;
	int32_t imm = (b7_6<<6) | (b5_3<<3);
	if((imm&0x0000080) != 0) {
		imm |= 0xffffff80;
	}
	return imm;
};

// C_SWSP - zero extended (6 bits << 2)
int32_t mk_swsp(uint16_t code) {
	int32_t b5_2 = (code>>9)&0xf;
	int32_t b7_6 = (code>>7)&0x3;
	int32_t imm = (b7_6<<6) | (b5_2<<2);
	return imm;
};


bool decodeCompressed(uint32_t addr, uint32_t code, Vliw* vlp) {
#if 1
	if(fastTable[code&0xffff] != NULL) {
		// printf("Fast: addr=0x%08x, code=0x%04x\n", addr, (uint16_t)code);
		Vliw* nvlp = fastTable[code&0xffff];
		// copy to new
		vlp->opcode = nvlp->opcode;
		vlp->rd = nvlp->rd;
		vlp->rs1 = nvlp->rs1;
		vlp->rs2 = nvlp->rs2;
		vlp->imm = nvlp->imm;
		if(DEBUG) {
			printf("Fast32: addr=0x%08x, code=0x%04x\n", addr, (uint16_t)code);
			printf("  vlp: %d, %d, %d, %d, %08x\n", vlp->opcode,
				vlp->rd, vlp->rs1, vlp->rs2, vlp->imm);

		}
		return true;
	}
#endif

	if(DEBUG || cpu->verbose) {
		printf("Comp32: addr=0x%08x, code=0x%04x\n", addr, (uint16_t)code);
	}
	// init the Vliw to zero
	vlp->opcode = VLIW_NOP;
	vlp->rd = 0;
	vlp->rs1 = 0;
	vlp->rs2 = 0;
	vlp->imm = 0;

	Optable* otp = optable;
	for(; otp->bitText != NULL; ++otp) {
		// printf("%s, 0x%08x, 0x%09x, %s\n", otp->bitText, otp->code, otp->mask, otp->asmText);
		if((code&otp->mask) == otp->code) {
			break;
		}
	}
	if(otp->bitText == NULL) {
		printf("Code: 0x%08x, not found\n", code);
		return false;
	}
	if(cpu->verbose) {
		printf("  %s, 0x%08x, 0x%08x, %s\n", otp->bitText, otp->code, otp->mask, otp->asmText);
	}

	vlp->opcode = otp->vliw;

	// rd', rs1', rs2'
	int r3b2 = regMap3[(code>>2)&0x7];	// rs2'
	int r3b7 = regMap3[(code>>7)&0x7];	// rd', rs1'

	switch(vlp->opcode) {

	//******************************************************
	//		Base, RV32
	//******************************************************
	// RV32/64 Base instructions

	// lw rd',offset(rs1')
	case VLIW_C_LW:
		vlp->opcode = VLIW_LW;
		vlp->rd = r3b2;
		vlp->rs1 = r3b7;
		vlp->rs2 = 0;
		vlp->imm = mk_sw(code);
		break;

	// sw rs2',offset(rs1')
	case VLIW_C_SW:
		vlp->opcode = VLIW_SW;
		vlp->rd = 0;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		vlp->imm = mk_sw(code);
		break;

#if 0
	// sd rs2',offset(rs1')
	case VLIW_C_FSW:			// RV32
		vlp->opcode = VLIW_SD;
		vlp->rd = 0;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		vlp->imm = mk_sd(code);
		break;
#endif

	// sw rs2,offset(sp)
	case VLIW_C_SWSP:
		vlp->opcode = VLIW_SW;
		vlp->rd = 0;
		vlp->rs1 = 2;	// sp
		vlp->rs2 = (code>>2)&0x1f;
		vlp->imm = mk_swsp(code);	// zero extended
		break;

	// beq rs1',x0,offset
	case VLIW_C_BEQZ:
		vlp->opcode = VLIW_BEQ;
		vlp->rd = 0;
		vlp->rs1 = r3b7;
		vlp->rs2 = 0;
		vlp->imm = mk_beq(code);
		break;

	// bne rs1',x0,offset
	case VLIW_C_BNEZ:
		vlp->opcode = VLIW_BNE;
		vlp->rd = 0;
		vlp->rs1 = r3b7;
		vlp->rs2 = 0;
		vlp->imm = mk_beq(code);
		break;

#if 0
	// add rd,x0,rs2
	case VLIW_C_MV:
		vlp->opcode = VLIW_ADD;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = 0;
		vlp->rs2 = (code>>2)&0x1f;
		vlp->imm = 0;
		break;
#else
	// addi rd,rs1,0, alternate move
	case VLIW_C_MV:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = (code>>2)&0x1f;
		vlp->rs2 = 0;
		vlp->imm = 0;
		break;
#endif

	// add rd,rd,rs2
	case VLIW_C_ADD:
		vlp->opcode = VLIW_ADD;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = (code>>2)&0x1f;;
		vlp->imm = 0;
		break;

	// addi rd, rd, nzimm
	case VLIW_C_ADDI:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_addi(code);
		break;

	// addi x0,x0,0
	case VLIW_C_NOP:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = 0;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = 0;
		break;

	case VLIW_C_ADDI16SP:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = 2;	// sp
		vlp->rs2 = 0;
		vlp->imm = mk_addi16sp(code);
		break;

	// addi rd',x2,nzuimm
	case VLIW_C_ADDI4SPN:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = r3b2;
		vlp->rs1 = 2;	// sp
		vlp->rs2 = 0;
		vlp->imm = mk_addi4spn(code);
		break;

	case VLIW_C_LWSP:
		vlp->opcode = VLIW_LW;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = 2;	// sp
		vlp->rs2 = 0;
		vlp->imm = mk_lwsp(code);
		break;

	case VLIW_C_LI:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = mk_imm6(code);
		break;

	case VLIW_C_LUI:
		vlp->opcode = VLIW_ADDI;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = mk_imm6(code)<<12;
		break;

	case VLIW_C_J:
		vlp->opcode = VLIW_JAL;
		// vlp->opcode = VLIW_J;
		vlp->rd = 0;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = jal_off(code);
		break;

	case VLIW_C_JAL:
		vlp->opcode = VLIW_JAL;
		vlp->rd = 1;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = jal_off(code);
		// printf("Jal: dest = %08x\n", addr+vlp->imm);
		break;

	// jalr x0,0(rs1)
	case VLIW_C_JR:
		vlp->opcode = VLIW_JALR;
		// vlp->opcode = VLIW_JR;
		vlp->rd = 0;
		vlp->rs1 = (code>>7)&0x1f;
		vlp->rs2 = 0;
		vlp->imm = 0;
		break;

	// jalr x1,0(rs1)
	case VLIW_C_JALR:
		vlp->opcode = VLIW_JALR;
		vlp->rd = 1;
		vlp->rs1 = (code>>7)&0x1f;
		vlp->rs2 = 0;
		vlp->imm = 0;
		break;

	case VLIW_C_SUB:
		vlp->opcode = VLIW_SUB;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;
	case VLIW_C_XOR:
		vlp->opcode = VLIW_XOR;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;

	// andi rd',rd',rs'
	case VLIW_C_OR:
		vlp->opcode = VLIW_OR;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;

	// andi rd',rd',rs'
	case VLIW_C_AND:
		vlp->opcode = VLIW_AND;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;

	// andi rd',rd',imm
	case VLIW_C_ANDI:
		vlp->opcode = VLIW_ANDI;
		vlp->rd = r3b7;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_addi(code);
		break;

		//******************************************************
		//		RV32
		//******************************************************
		// RV32 specific instructions
#if RV32
	// slli rd, rd, shamt
	case VLIW_C_SLLI:
		vlp->opcode = VLIW_SLLI;
		vlp->rd = regMap3[(code>>7)&0x7];
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_shamt(code);
		break;

	// srli rd, rd, shamt
	case VLIW_C_SRLI:
		vlp->opcode = VLIW_SRLI;
		vlp->rd = r3b7;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_shamt(code);
		break;

	// srai rd, rd, shamt
	case VLIW_C_SRAI:
		vlp->opcode = VLIW_SRAI;
		vlp->rd = r3b7;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_shamt(code);
		break;
#endif
		//******************************************************
		//		RV64
		//******************************************************
#if RV64
	// addiw rd,rd,imm
	case VLIW_C_ADDIW:
		vlp->opcode = VLIW_ADDIW;
		vlp->rd = (code>>7)&0x1f;
		vlp->rs1 = vlp->rd;
		vlp->rs2 = 0;
		vlp->imm = mk_addi(code);
		break;

	case VLIW_C_ADDW:
		vlp->opcode = VLIW_ADDW;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;
	case VLIW_C_SUBW:
		vlp->opcode = VLIW_SUBW;
		vlp->rd = r3b7;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		break;

	// sw rs2',offset(rs1')
	case VLIW_C_LD:
		vlp->opcode = VLIW_LD;
		vlp->rd = r3b2;
		vlp->rs1 = r3b7;
		vlp->rs2 = 0;
		vlp->imm = mk_sd(code);
		break;

	// sd rs2',offset(rs1')
	case VLIW_C_SD:				// RV64
		vlp->opcode = VLIW_SD;
		vlp->rd = 0;
		vlp->rs1 = r3b7;
		vlp->rs2 = r3b2;
		vlp->imm = mk_sd(code);
		break;
#endif

	// c.ebreak
	case VLIW_C_EBREAK:
		vlp->opcode = VLIW_EBREAK;
		vlp->rd = 0;
		vlp->rs1 = 0;
		vlp->rs2 = 0;
		vlp->imm = 0;
		break;

	default:
		printf("Not found: %s, 0x%08x, 0x%08x, %s\n", otp->bitText, otp->code, otp->mask, otp->asmText);
		exit(-1);
		break;
	}

	// vlp->dump();
	// cpu->dump32();
	if (DEBUG) {
		printf("  vlp: %d, %d, %d, %d, %08x\n", vlp->opcode,
			vlp->rd, vlp->rs1, vlp->rs2, vlp->imm);
	}

	// update fast table
	if(fastTable[code&0xffff] == NULL) {
		Vliw* nvlp = new Vliw;
		// copy to new
		nvlp->opcode = vlp->opcode;
		nvlp->rd = vlp->rd;
		nvlp->rs1 = vlp->rs1;
		nvlp->rs2 = vlp->rs2;
		nvlp->imm = vlp->imm;
		fastTable[code&0xffff] = nvlp;
	}
	return true;
}

