#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RiscVFast.h"

// #include "rv_csr.cpp"


// 0000000tttttsssss000ddddd0110011	0x00000033	0xfe00707f	rv32,rv64	I-type	add rd,rs1,rs2	rd <= rs1 + rs2

const char* rn[32] {
	"zero", "ra", "sp", "gp",
	"tp", "t0", "t1", "t2",
	"s0", "s1", "a0", "a1",
	"a2", "a3", "a4", "a5",

	"a6", "a7", "s2", "s3",
	"s4", "s5", "s6", "s7",
	"s8", "s9", "s10", "s11",
	"t3", "t4", "t5", "t6"
};


const char* alu_name[8] {
	"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"
};
const char* alu2_name[8] {
	"sub", "???", "???", "???", "???", "sra", "???", "???"
};
const char* alui_name[8] {
	"addi", "slli", "slti", "sltiu", "xori", "srli", "ori", "andi"
};
const char* alui2_name[8] {
	"???", "???", "???", "???", "???", "srai", "???", "???"
};
const char* store_name[8] {
	"sb", "sh", "sw", "???", "???", "???", "???", "???"
};
const char* load_name[8] {
	"lb", "lh", "lw", "???", "lbu", "lhu", "???", "???"
};
const char* bra_name[8] {
	"beq", "bne", "???", "???", "blt", "bge", "bltu", "bgeu"
};
const char* braz_name[8] {
	"beqz", "bnez", "???", "???", "bltz", "bgez", "bltuz", "bgeuz"
};
const char* brazi_name[8] {
	"???", "???", "???", "???", "bgtz", "blez", "???", "???"
};
const char* csr_opname[8] {
	"???", "csrrw", "csrrs", "csrrc", "???", "csrrwi", "csrrsi", "csrrci"
};

const char* mul_name[8] {
	"mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"
};


void disasm(uint32_t pc, uint32_t insn, char* buf) {
	buf[0] = '\0';
	int opcode = insn&0x7f;
	int rd = (insn>>7)&0x1f;
	int rs1 = (insn>>15)&0x1f;
	int rs2 = (insn>>20)&0x1f;
	int type = (insn>>12)&0x7;
	int insn_31_25 = (insn>>25)&0x7f;
	int imm12 = (((int32_t)insn)>>20);

	switch(opcode) {
	// ALU
	case 0x33: {
		if(insn_31_25 == 0x00) {
			// sltu a2,zero,a0 => snez a4,a0
			if(type == 3 && rs1 == 0) {
				sprintf(buf, "snez %s,%s", rn[rd], rn[rs2]);
			}
			else {
				// slt a4,zero,a5 => sgtz a4,a5
				if(type == 2 && rs1 == 0) {
					sprintf(buf, "sgtz %s,%s", rn[rd], rn[rs2]);
				}
				else {
					sprintf(buf, "%s %s,%s,%s", alu_name[type], rn[rd], rn[rs1], rn[rs2]);
				}
			}
		}
		else if(insn_31_25 == 0x20) {
			if(type == 0 && rs1 == 0) {
				sprintf(buf, "neg %s,%s", rn[rd], rn[rs2]);
			}
			else {
				sprintf(buf, "%s %s,%s,%s", alu2_name[type], rn[rd], rn[rs1], rn[rs2]);
			}
		}
		else if(insn_31_25 == 0x01) {
			sprintf(buf, "%s %s,%s,%s", mul_name[type], rn[rd], rn[rs1], rn[rs2]);
		}
		else {
			sprintf(buf, "???");
		}
	}
	break;

	// ALUI
	case 0x13: {
		if(insn == 0x00000013) {
			sprintf(buf, "nop");
		}
		else if(type == 0 && rs1 == 0) {
			sprintf(buf, "li %s,%d", rn[rd], imm12);
		}
		else if(type == 0 && imm12 == 0) {
			sprintf(buf, "mv %s,%s", rn[rd], rn[rs1]);
		}
		else if(type == 1 || type == 5) {
			if(insn_31_25 == 0x00) {
				sprintf(buf, "%s %s,%s,0x%x", alui_name[type], rn[rd], rn[rs1], imm12&0x1f);
			}
			else if(insn_31_25 == 0x20) {
				sprintf(buf, "%s %s,%s,0x%x", alui2_name[type], rn[rd], rn[rs1], imm12&0x1f);
			}
			else {
				sprintf(buf, "???");
			}
		}
		else {
			// xori a1,a2,-1 => not a1,a2
			if(type == 4 && imm12 == -1) {
				sprintf(buf, "not %s,%s", rn[rd], rn[rs1]);
			}
			// andi s2,s2.255 => zext.b s2,s2
			else if(type == 7 && imm12 == 255) {
				sprintf(buf, "zext.b %s,%s", rn[rd], rn[rs1]);
			}
			// sltiu a4,a0,1 => seqz a4,a0
			else if(type == 3 && imm12 == 1) {
				sprintf(buf, "seqz %s,%s", rn[rd], rn[rs1]);
			}
			else {
				sprintf(buf, "%s %s,%s,%d", alui_name[type], rn[rd], rn[rs1], imm12);
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
		sprintf(buf, "%s %s,%d(%s)", store_name[type], rn[rs2], imm32_S, rn[rs1]);
	}
	break;

	// Load
	// wire [31:0] imm32_I = { {20{insn[31]}}, insn[31:25], insn[24:21], insn[20] };
	case 0x03: {
		int32_t imm32_load = ((int32_t)insn)>>20;
		sprintf(buf, "%s %s,%d(%s)", load_name[type], rn[rd], imm32_load, rn[rs1]);
	}
	break;

	// LUI
	case 0x37: {
		int32_t imm32_lui = ((uint32_t)insn)>>12;
		sprintf(buf, "lui %s,0x%x", rn[rd], imm32_lui);
	}
	break;

	// AUIPC
	case 0x17: {
		int32_t imm32_auipc = ((uint32_t)insn)>>12;
		sprintf(buf, "auipc %s,0x%x", rn[rd], imm32_auipc);
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
		// sprintf(buf, "=> pc = %08x, imm32_j = %08x, pc+imm32_j = %08x", pc, imm32_j, pc+imm32_j);
		if(rd == 0) {
			sprintf(buf, "j %x", pc+imm32_j);
		}
		else {
			sprintf(buf, "jal %s,%x", rn[rd], pc+imm32_j);
		}
	}
	break;

	// JALR
	case 0x67: {
		if(insn == 0x00008067) {
			sprintf(buf, "ret");
		}
		else if(rd == 0 && imm12==0) {
			sprintf(buf, "jr %s", rn[rs1]);
		}
		else if(rd == 1 && imm12==0) {
			sprintf(buf, "jalr %s", rn[rs1]);
		}
		else if(rd == 0) {
			sprintf(buf, "jr %d(%s)", imm12, rn[rs1]);
		}
		else if(rd == 1) {
			sprintf(buf, "jalr %d(%s)", imm12, rn[rs1]);
		}
		else {
			sprintf(buf, "jalr %s,%d(%s)", rn[rd], imm12, rn[rs1]);
		}
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
		if(rs2 == 0) {
			sprintf(buf, "%s %s,%x", braz_name[type], rn[rs1], pc+simm12);
		}
		else if(rs1 == 0) {
			sprintf(buf, "%s %s,%x", brazi_name[type], rn[rs2], pc+simm12);
		}
		else {
			sprintf(buf, "%s %s,%s,%x", bra_name[type], rn[rs1], rn[rs2], pc+simm12);
		}
	}
	break;


	// SYSTEM
	case 0x73: {
		if(type == 0 || type == 4) {
			if(insn == 0x00000073) {
				sprintf(buf, "ecall");
			}
			else if(insn == 0x00100073) {
				sprintf(buf, "ebreak");
			}
			else if(insn == 0x10200073) {
				sprintf(buf, "sret");
			}
			else if(insn == 0x30200073) {
				sprintf(buf, "mret");
			}
			else if(insn == 0x10500073) {
				sprintf(buf, "wfi");
			}
			else {
				sprintf(buf, "???");
			}
		}
		else {
			int csr = ((uint32_t)insn)>>20;
			// csrrw mepc,zero,a5 => csrw mepc,a5
			if(type == 1 && rd == 0) {
				sprintf(buf, "csrw %s,%s", csr_name(csr), rn[rs1]);
			}
			// csrrs mepc,zero,a5 => csrr a5,mepc
			else if(type == 2 && rs1 == 0) {
				if(csr == 0xc01) {
					sprintf(buf, "rdtime %s", rn[rd]);
				}
				else if(csr == 0xc81) {
					sprintf(buf, "rdtimeh %s", rn[rd]);
				}
#if 0	// Custom rand CSR
				// csrr a1,rand => csrr a1,0xc00
				else if(csr == 0xcc0) {
					sprintf(buf, "csrr %s,0x%03x", rn[rd], csr);
				}
#endif
				else {
					sprintf(buf, "csrr %s,%s", rn[rd], csr_name(csr));
				}
			}
			// csrrc mip,zero,a0 => csrc mip,a0
			else if(type == 3 && rd == 0) {
				sprintf(buf, "csrc %s,%s", csr_name(csr), rn[rs1]);
			}
			else if(type < 4) {
				sprintf(buf, "%s %s,%s,%s", csr_opname[type], csr_name(csr), rn[rd], rn[rs1]);
			}
			else {
				sprintf(buf, "%s %s,%s,%x", csr_opname[type], csr_name(csr), rn[rd], rs1);
			}
		}

	}
	break;

	default:
		return;
	}
}

