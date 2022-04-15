// RiscV Compile - compile RiscV Instruction to Dreaded Threaded C code

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "RiscVFast.h"
#include "Cpu.h"
#include "Elf64.h"

#include "riscv_csr.h"


FILE* ofp = NULL;
bool add_debug = false;
bool add_check = true;
bool add_realtime = true;	// pseudo real time emulation

void rv_compile(const char* outfile, uint32_t addr, uint32_t len) {
	printf("rv_compile: base=0x%08x, len=0x%08x\n", addr, len);

	ofp = fopen(outfile, "w");
	if(ofp == NULL) {
		fatal("Can not open %s\n", outfile);
		return;
	}

	fprintf(ofp, "next:\n");
	if(add_debug) {
		fprintf(ofp, "printf(\"pc: %%08x\\n\", pc);\n");
		fprintf(ofp, "dump();\n");
	}
	if(add_check) {
		fprintf(ofp, "if (r[0] != 0) {\n");
		fprintf(ofp, "printf(\"Bad x0 register!!!\\n\");\n");
		fprintf(ofp, "dump();\n");
		fprintf(ofp, "}\n");
		fprintf(ofp, "if (r[2] < 0x40000000 || r[2] > 0x40010000) {\n");
		fprintf(ofp, "printf(\"Bad sp register!!!\\n\");\n");
		fprintf(ofp, "dump();\n");
		fprintf(ofp, "}\n");
		fprintf(ofp, "if ((nonseq++&0xffffff) == 0) {\n");
		fprintf(ofp, "fputc('.', stderr); fflush(stderr);\n");
		fprintf(ofp, "}\n");
	}
	if(add_realtime) {
		fprintf(ofp, "++cycle;\n");
	}
	fprintf(ofp, "switch(pc>>2) {\n");
	for(uint32_t i=addr; i<addr+len; i += 4) {
		fprintf(ofp, "case 0x%x: goto L_%x;\n", i>>2, i);
	}
	fprintf(ofp, "}\n");

	fprintf(ofp, "printf(\"Bad pc: 0x%%08x\\n\", pc);\n");	// printf("Bad pc: 0x%08x\n", pc);
	fprintf(ofp, "return;\n");	// return;


	for(uint32_t i=addr; i<addr+len; i += 4) {
		rv_insn(i, memory->read32(i));
	}
	fclose(ofp);
}


void rv_insn(uint32_t pc, uint32_t insn) {
	static char buf[256];
	int opcode = insn&0x7f;
	int rd = (insn>>7)&0x1f;
	int rs1 = (insn>>15)&0x1f;
	int rs2 = (insn>>20)&0x1f;
	int type = (insn>>12)&0x7;
	int insn_31_25 = (insn>>25)&0x7f;
	int imm12 = (((int32_t)insn)>>20);

	// strings for register sources
	char sd[8];
	char ss1[8];
	char ss2[8];
	if(rd == 0) {
		sprintf(sd, "0");
	}
	else {
		sprintf(sd, "r[%d]", rd&0x1f);
	}
	if(rs1 == 0) {
		sprintf(ss1, "0");
	}
	else {
		sprintf(ss1, "r[%d]", rs1&0x1f);
	}
	if(rs2 == 0) {
		sprintf(ss2, "0");
	}
	else {
		sprintf(ss2, "r[%d]", rs2&0x1f);
	}

	// every instruction gets a label
	fprintf(ofp, "\nL_%x:\n", pc);

	buf[0] = '\0';

	// show Instruction
	disasm(pc, insn, buf);
	fprintf(ofp, "\t// %08x: %08x %s\n", pc, insn, buf);

	switch(opcode) {
	// ALU
	case 0x33: {
		//================================================
		// Compile Instruction
		//================================================
		// "add", "sll", "slt", "sltu", "xor", "srl", "or", "and"
		if(insn_31_25 == 0x00) {
			switch(type) {
			case 0:	// add
				fprintf(ofp, "\t%s = %s + %s;\n", sd, ss1, ss2);
				break;

			case 1:	// sll
				fprintf(ofp, "\tr[%d] = r[%d]<<(r[%d]&0x1f);\n", rd, rs1, rs2);
				break;

			case 2:	// slt
				fprintf(ofp, "\tr[%d] = (((int32_t)r[%d])<((int32_t)r[%d]))?1:0;\n", rd, rs1, rs2);
				break;

			case 3:	// sltu
				fprintf(ofp, "\tr[%d] = (((uint32_t)r[%d])<((uint32_t)r[%d]))?1:0;\n", rd, rs1, rs2);
				break;

			case 4:	// xor
				fprintf(ofp, "\tr[%d] = r[%d] ^ r[%d];\n", rd, rs1, rs2);
				break;

			case 5:	// srl
				fprintf(ofp, "\tr[%d] = ((uint32_t)r[%d])>>(r[%d]&0x1f);\n", rd, rs1, rs2);
				break;

			case 6:	// or
				fprintf(ofp, "\tr[%d] = r[%d] | r[%d];\n", rd, rs1, rs2);
				break;

			case 7:	// and
				fprintf(ofp, "\tr[%d] = r[%d] & r[%d];\n", rd, rs1, rs2);
				break;
			}
		}
		// "sub", "???", "???", "???", "???", "sra", "???", "???"
		else if(insn_31_25 == 0x20) {
			switch(type) {
			case 0:		// sub
				fprintf(ofp, "\tr[%d] = r[%d] - r[%d];\n", rd, rs1, rs2);
				break;

			case 5:	// sra
				fprintf(ofp, "\tr[%d] = ((int32_t)r[%d])>>(r[%d]&0x1f);\n", rd, rs1, rs2);
				break;
			}
		}
		else if(insn_31_25 == 0x01) {
			switch(type) {
			default:		//
				// fprintf(ofp, "\tr[%d] = r[%d] + r[%d];\n", rd, rs1, rs2);
				fprintf(ofp, "\t// TO DO !!!!\n");
				break;
			}
		}
	}
	break;

	// ALUI
	case 0x13: {
		//================================================
		// Compile Instruction
		//================================================
		if(type == 5 && insn_31_25 == 0x20) { // srai
			fprintf(ofp, "\tr[%d] = ((int32_t)r[%d])>>0x%x;\n", rd, rs1, imm12&0x1f);
		}
		else {
			// "addi", "slli", "slti", "sltiu", "xori", "srli", "ori", "andi"
			switch(type) {
			case 0:	// addi
				if(insn == 0x00000013) {
					fprintf(ofp, "\t// NOP;\n");
				}
				else if(imm12 == 0) {
					fprintf(ofp, "\t%s = %s;\n", sd, ss1);
				}
				else if(rs1 == 0) {
					fprintf(ofp, "\t%s = %d;\n", sd, imm12);
				}
				else {
					fprintf(ofp, "\t%s = %s + %d;\n", sd, ss1, imm12);
				}
				break;

			case 1:	// slli
				fprintf(ofp, "\tr[%d] = r[%d]<<0x%x;\n", rd, rs1, imm12&0x1f);
				break;

			case 2:	// slti
				fprintf(ofp, "\tr[%d] = (((int32_t)r[%d])<(int32_t)%d)?1:0;\n", rd, rs1, imm12);
				break;

			case 3:	// sltui
				fprintf(ofp, "\tr[%d] = ((uint32_t)r[%d]<(uint32_t)%d)?1:0;\n", rd, rs1, imm12);
				break;

			case 4:	// xori
				fprintf(ofp, "\tr[%d] = r[%d] ^ %d;\n", rd, rs1, imm12);
				break;

			case 5:	// srli
				fprintf(ofp, "\tr[%d] = ((uint32_t)r[%d])>>0x%x;\n", rd, rs1, imm12&0x1f);
				break;

			case 6:	// ori
				fprintf(ofp, "\tr[%d] = r[%d] | %d;\n", rd, rs1, imm12);
				break;

			case 7:	// andi
				fprintf(ofp, "\tr[%d] = r[%d] & %d;\n", rd, rs1, imm12);
				break;
			}
		}
	}
	break;

	// Store
	// S-immediate
	// wire [31:0] imm32_S = { {20{insn[31]}}, insn[31:25], insn[11:8], insn[7] } };
	case 0x23: {
		uint32_t f1 = (((int32_t)insn)>>20)&0xffffffe0;
		uint32_t f2 = (insn>>7)&0x0000001f;
		int32_t imm32_S = f1 | f2;

		//================================================
		// Compile Instruction
		//================================================
		// "sb", "sh", "sw", "???", "???", "???", "???", "???"
		switch(type) {
		case 0:	// sb
			fprintf(ofp, "\tmemory->write8(r[%d]+%d, r[%d]);\n", rs1, imm32_S, rs2);
			break;
		case 1:	// sh
			fprintf(ofp, "\tmemory->write16(r[%d]+%d, r[%d]);\n", rs1, imm32_S, rs2);
			break;
		case 2:	// sw
			if(0 || add_debug) {
				fprintf(ofp, "if ( ((r[%d]+%d)&0xf0000000) == 0xf0000000) ", rs1, imm32_S);
				fprintf(ofp, "\tprintf(\"%08x: %%08x <= %%08x\\n\", r[%d]+%d, r[%d]);\n",
					pc, rs1, imm32_S, rs2);
			}
			fprintf(ofp, "\tmemory->write32(r[%d]+%d, r[%d]);\n", rs1, imm32_S, rs2);
			break;
		}
	}
	break;

	// Load
	// wire [31:0] imm32_I = { {20{insn[31]}}, insn[31:25], insn[24:21], insn[20] };
	case 0x03: {
		int32_t imm32_load = ((int32_t)insn)>>20;

		//================================================
		// Compile Instruction
		//================================================
		// "lb", "lh", "lw", "???", "lbu", "lhu", "???", "???"
		switch(type) {
		case 0:	// lb
			fprintf(ofp, "\tr[%d] = memory->read8(r[%d]+%d);\n", rd, rs1, imm32_load);
			break;
		case 1:	// lh
			fprintf(ofp, "\tr[%d] = memory->read16(r[%d]+%d);\n", rd, rs1, imm32_load);
			break;
		case 2:	// lw
			fprintf(ofp, "\tr[%d] = memory->read32(r[%d]+%d);\n", rd, rs1, imm32_load);
			if(0 || add_debug) {
				// fprintf(ofp, "if ( ((r[%d]+%d)&0xf0000000) == 0xf0000000) ", rs1, imm32_S);
				fprintf(ofp, "\tprintf(\"%08x: %%08x => %%08x\\n\", r[%d]+%d, r[%d]);\n",
					pc, rs1, imm32_load, rd);
				fprintf(ofp, "\tdump();\n");
			}
			break;
		case 4:	// lbu
			fprintf(ofp, "\tr[%d] = memory->read8u(r[%d]+%d);\n", rd, rs1, imm32_load);
			break;
		case 5:	// lhu
			fprintf(ofp, "\tr[%d] = memory->read16u(r[%d]+%d);\n", rd, rs1, imm32_load);
			break;
		}
	}
	break;

	// LUI
	case 0x37: {
		//================================================
		// Compile Instruction
		//================================================
		fprintf(ofp, "\tr[%d] = 0x%08x;\n", rd, insn&0xfffff000);
	}
	break;

	// AUIPC
	case 0x17: {
		//================================================
		// Compile Instruction
		//================================================
		if(rd == 0) {
			error("Illegal X0 usage!\n");
		}
		fprintf(ofp, "\tr[%d] = 0x%08x;\n", rd, pc+(insn&0xfffff000));
	}
	break;

	// JAL
	// J-immediate
	// wire [31:0] imm32_J = { {12{insn[31]}}, insn[19:12], insn[20], insn[30:25], insn[24:21], 1'b0 };
	case 0x6f: {
		uint32_t field = (((int32_t)insn)>>12) &0x000fffff;
		uint32_t f1 = (field>>0)&0xff;	// [19:12]
		uint32_t f2 = (field>>8)&0x01;	// 11
		uint32_t f3 = (field>>9)&0x3ff;	// [10:1]
		uint32_t f4 = (field>>19)&0x01;	// 20
		int32_t imm32_j = (f4<<20) | (f3<<1) | (f2<<11) | (f1<<12);
		imm32_j = (imm32_j<<11)>>11;	// sign extend

		//================================================
		// Compile Instruction
		//================================================
		uint32_t save_pc = pc+4;
		fprintf(ofp, "\tpc = 0x%08x;\n", pc+imm32_j);
		if(rd != 0) {
			fprintf(ofp, "\tr[%d] = 0x%08x;\n", rd, save_pc);
		}
		fprintf(ofp, "\tgoto next;\n");
	}
	break;

	// JALR
	case 0x67: {
		//================================================
		// Compile Instruction
		//================================================
		uint32_t save_pc = pc+4;
		if(imm12 == 0) {
			fprintf(ofp, "\tpc = r[%d];\n", rs1);
		}
		else {
			fprintf(ofp, "\tpc = r[%d] + %d;\n", rs1, imm12);
		}
		if(rd != 0) {
			fprintf(ofp, "\tr[%d] = 0x%08x;\n", rd, save_pc);
		}
		fprintf(ofp, "\tgoto next;\n");
	}
	break;

	// BRA
	// B-immediate
	// wire [31:0] imm32_B = { {19{insn[31]}}, insn[7], insn[30:25], insn[11:8], 1'b0 };
	case 0x63: {
		uint32_t f1 = (insn>>25) &0x0000007f;
		uint32_t b1 = (f1>>6)&0x01;	// [12]
		uint32_t b2 = (f1>>0)&0x3f;	// [10:5]
		uint32_t f2 = (insn>>7) &0x0000007f;
		uint32_t b3 = (f2>>1)&0x0f;	// [4:1]
		uint32_t b4 = (f2>>0)&0x01;	// [11]
		int32_t simm12 = (b1<<12) | (b2<<5) | (b3<<1) | (b4<<11);
		simm12 = (simm12<<19)>>19;

		//================================================
		// Compile Instruction
		//================================================
		// 	"beq", "bne", "???", "???", "blt", "bge", "bltu", "bgeu"
		switch(type) {
		case 0:	// beq
			fprintf(ofp, "\tif(%s == %s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		case 1:	// bne
			fprintf(ofp, "\tif(%s != %s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		case 4:	// blt
			fprintf(ofp, "\tif((int32_t)%s < (int32_t)%s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		case 5:	// bge
			fprintf(ofp, "\tif((int32_t)%s >= (int32_t)%s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		case 6:	// bltu
			fprintf(ofp, "\tif((uint32_t)%s < (uint32_t)%s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		case 7:	// bgeu
			fprintf(ofp, "\tif((uint32_t)%s >= (uint32_t)%s) goto L_%x;\n", ss1, ss2, pc+simm12);
			break;
		}
	}
	break;


	// SYSTEM
	case 0x73: {
		//================================================
		// Compile Instruction
		//================================================
		if(type == 0 || type == 4) {
			if(insn == 0x00000073) {		// ecall
				if(do_newlib) {
					fprintf(ofp, "\tsyscall();\n");
				}
				else {
					fprintf(ofp, "\tecall();\n");
				}
			}
			else if(insn == 0x00100073) {	// ebreak
				fprintf(ofp, "\tebreak();\n");
			}
			else if(insn == 0x10200073) {	// sret
				fprintf(ofp, "\tsret();\n");
			}
			else if(insn == 0x30200073) {	// mret
				fprintf(ofp, "\tmret();\n");
			}
			else if(insn == 0x10500073) {	// wfi
				fprintf(ofp, "\twfi();\n");
			}
			else {
				fprintf(ofp, "\t// Unknown System Instruction - ???");
			}
		}
		else {
			int csr = ((uint32_t)insn)>>20;
			if(type < 4) {
				fprintf(ofp, "\tcsr_insn(%d, 0x%03x, %d, %d, r[%d]);\n",
					type&0x3, csr, rd, rs1, rs1);
			}
			else {
				fprintf(ofp, "\tcsr_insn(%d, 0x%03x, %d, %d, 0x%x);\n",
					type&0x3, csr, rd, rs1, rs1);
			}
		}
	}
	break;

	default:
		return;
	}
}
