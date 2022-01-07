#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>

#include "RiscVFast.h"
#include "Cpu.h"

#if RV64
#include "OpTable_rv64.cpp"
#endif
#if RV32
#include "OpTable_rv32.cpp"
#endif


const char* regNameTable[32] {
	// "zero", "ra", "sp", "gp",
	"x0", "ra", "sp", "gp",
	"tp", "t0", "t1", "t2",
	"s0", "s1", "a0", "a1",
	"a2", "a3", "a4", "a5",

	"a6", "a7", "s2", "s3",
	"s4", "s5", "s6", "s7",
	"s8", "s9", "s10", "s11",
	"t3", "t4", "t5", "t6"
};

const char* getRegName(int n) {
	if(n <0 || n > 32) {
		return "??";
	}
	return regNameTable[n];
};


bool streq(const char* s, const char* t) {
	return (strcmp(s, t) == 0);
}

// "20|[10:1]|11|[19:12]"
uint32_t doImm(uint32_t code, const char* str) {
	uint32_t imm = 0;
	const char* s = str;
	for(;;) {
		if(*s == '\0') {
			break;
		}
		if(*s == '|') {
			++s;
		}
		// single number
		else if(isdigit(*s)) {
			int n = atoi(s);
			while(isdigit(*s)) {
				++s;
			}
			printf("n = %d\n", n);
			imm |= (1<<n);
		}
		// range [n1:n2]
		else if(*s == '[') {
			++s;
			int n1 = atoi(s);
			while(isdigit(*s)) {
				++s;
			}
			if(*s != ':') {
				printf("doImm: Oops, expecting ':', got '%c', [d1:d2]; %s\n", *s, str);
				return 0;
			}
			++s;

			int n2 = atoi(s);
			while(isdigit(*s)) {
				++s;
			}
			if(*s != ']') {
				printf("doImm: Oops, expecting ']', got '%c', [d1:d2]; %s\n", *s, str);
				return 0;
			}
			++s;

			printf("n1/n2 = [%d:%d]\n", n1, n2);
			// imm |= 1<<n;
		}
		else {
			printf("doImm: Oops, bad format '%c'; %s\n", *s, str);
			return 0;
		}
	}
	return imm;
}


// J-type Immediate "20|[10:1]|11|[19:12]"
uint32_t doJoff(uint32_t code) {
	uint32_t field = (code>>12) &0x000fffff;
	uint32_t f1 = (field>>0)&0xff;	// [19:12]
	uint32_t f2 = (field>>8)&0x01;	// 11
	uint32_t f3 = (field>>9)&0x3ff;	// [10:1]
	uint32_t f4 = (field>>19)&0x01;	// 20
	return (f4<<20) | (f3<<1) | (f2<<11) | (f1<<12);
}

// B-type Immediate {[12],[10:5]}@25, {[4:1],[11]}@7
uint32_t doBoff(uint32_t code) {
	uint32_t f1 = (code>>25) &0x0000007f;
	uint32_t b1 = (f1>>6)&0x01;	// [12]
	uint32_t b2 = (f1>>0)&0x3f;	// [10:5]
	uint32_t f2 = (code>>7) &0x0000007f;
	uint32_t b3 = (f2>>1)&0x0f;	// [4:1]
	uint32_t b4 = (f2>>0)&0x01;	// [11]
	uint32_t imm12 = (b1<<12) | (b2<<5) | (b3<<1) | (b4<<11);
	if(imm12&0x00001000) {
		imm12 |= 0xfffff000;
	}
	return imm12;
}

// S-type Immediate {[11:0]}@20
uint32_t mk_simm12(uint32_t code) {
	uint32_t imm12 = (code>>20)&0x00000fff;
	// sign extend
	if(imm12&0x00000800) {
		imm12 |= 0xfffff000;
	}
	return imm12;
}

// S-type Immediate {[11:5]}@25, {[4:0]}@7
uint32_t doS2imm12(uint32_t code) {
	uint32_t f1 = (code>>25) &0x0000007f;
	uint32_t f2 = (code>>7) &0x0000001f;
	uint32_t imm12 = (f1<<5) | f2;
	// sign extend
	if(imm12&0x00000800) {
		imm12 |= 0xfffff000;
	}
	return imm12;
}


bool decode(uint32_t addr, uint32_t code, Vliw* vlp) {
	if(cpu->verbose) {
		printf("decode: 0x%08x - 0x%08x\n", addr, code);
	}

	if((code&0x00000003) != 0x00000003) {
		return decodeCompressed(addr, code, vlp);
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

	// break asmText into parts
	char text[256];
	char out[256];
	char* item[16];
	strcpy(text, otp->asmText);
	int n = 0;
	char* s=text;
	// memset(out, 0, sizeof(out));
	out[0] = '\0';

	for(;;) {
		item[n++] = s;
		for(;;) {
			if(*s == '\0' || *s == ',' || isspace(*s)) {
				break;
			}
			++s;
		}
		if(*s == ',' || isspace(*s)) {
			*s++ = '\0';
		}
		if(*s == '\0') {
			break;
		}
	}

	for(int i=0; i<n; ++i) {
		char buf[16];

		if(item[i] == NULL) {
#ifdef __linux__
			printf("%s: %d: Oops: NULL\n", __func__, __LINE__);
#else
			printf("%s: %d: Oops: NULL\n", __FUNC__, __LINE__);
#endif
		}
		// printf("[%d:%d] '%s'\n", i+1, n, item[i]);

		if(i == 0) {
			strcpy(out, item[0]);
			strcat(out, " ");
		}

		// lookup actual bits
		else if(streq(item[i], "rd")) {
			int field = (code>>7)&0x1f;
			// sprintf(buf, "r%d", field);
			// printf("Rd = %d\n", field);
			sprintf(buf, "%s", getRegName(field));
			vlp->rd = field;
			strcat(out, buf);
		}
		else if(streq(item[i], "rs") || streq(item[i], "rs1")) {
			int field = (code>>15)&0x1f;
			// sprintf(buf, "r%d", field);
			sprintf(buf, "%s", getRegName(field));
			vlp->rs1 = field;
			strcat(out, buf);
		}
		else if(streq(item[i], "rs2")) {
			int field = (code>>20)&0x1f;
			// sprintf(buf, "r%d", field);
			sprintf(buf, "%s", getRegName(field));
			vlp->rs2 = field;
			strcat(out, buf);
		}
		else if(streq(item[i], "shamt")) {
			int field = (code>>20)&0x1f;	// 5 bit
			sprintf(buf, "%d", field);
			vlp->imm = field;
			strcat(out, buf);
		}
		else if(streq(item[i], "shamt6")) {
			int field = (code>>20)&0x3f;	// 6 bit
			sprintf(buf, "%d", field);
			vlp->imm = field;
			strcat(out, buf);
		}

		// B-type immediate
		else if(streq(item[i], "boff")) {
			uint32_t imm12 = doBoff(code);
			sprintf(buf, "0x%08x", addr+imm12);
			vlp->imm = imm12;
			strcat(out, buf);
		}
		// J-type Immediate
		// imm[20] imm[10:1] imm[11] imm[19:12] rd opcode J-type
		else if(streq(item[i], "joff")) {
			uint32_t imm20 = doJoff(code);
			// sign extend
			if(imm20 & 0x00100000) {
				imm20 |= 0xffe00000;
			}
			vlp->imm = imm20;
			sprintf(buf, "0x%08x", addr+imm20);
			strcat(out, buf);
		}

		// S-type immediate
		else if(streq(item[i], "simm12")) {
			uint32_t imm12 = mk_simm12(code);
			vlp->imm = imm12;
			sprintf(buf, "0x%03x", imm12);
			strcat(out, buf);
		}
		// S-type immediate
		else if(streq(item[i], "s2imm12")) {
			uint32_t imm12 = doS2imm12(code);
			vlp->imm = imm12;
			sprintf(buf, "0x%03x", imm12);
			strcat(out, buf);
		}
		// LUI, AUI
		// imm[31:10]
		else if(streq(item[i], "imm20")) {
			uint32_t imm = (code>>12)&0x000fffff;
			// sign extend
			if(imm & 0x00080000) {
				imm |= 0xfff00000;
			}
			vlp->imm = imm;
			sprintf(buf, "0x%05x", imm);
			strcat(out, buf);
		}
		else if(streq(item[i], "imm12")) {
			uint32_t imm = (code>>12)&0x000fffff;
			sprintf(buf, "0x%03x", imm);
			vlp->imm = imm;
			strcat(out, buf);
		}
		else if(streq(item[i], "uimm5")) {
			uint32_t uimm5 = (code>>15)&0x1f;
			sprintf(buf, "0x%02x", uimm5);
			vlp->rs1 = uimm5;
			strcat(out, buf);
		}
		else if(streq(item[i], "csr")) {
			uint32_t csr = (code>>20)&0x00000fff;
			sprintf(buf, "0x%03x", csr);
			vlp->imm = csr;
			strcat(out, buf);
		}
		else {
			strcat(out, item[i]);
			fatal("Decode: Unknown item %s\n", item[i]);
		}
		if(i > 0 && i < n-1) {
			strcat(out, ",");
		}

	}
	if(cpu->verbose) {
		printf("  %08x => %s\n", addr, out);
	}
	return true;
}


void execute(uint32_t addr, uint32_t code) {
	Vliw* vlp= new Vliw;
	decode(addr, code, vlp);
	vlp->dump();

	cpu->pc = addr;
	cpu->execute(vlp);
}



/* testing

//   1007c:	00000793          	li	a5,0
//   10080:	00078863          	beqz	a5,10090 <register_fini+0x14>
//   10084:	00010537          	lui	a0,0x10
//   10088:	42850513          	addi	a0,a0,1064 # 10428 <__libc_fini_array>
//   1008c:	3880006f          	j	10414 <atexit>
//   10090:	00008067          	ret

int main(int argc, char** argv) {
	Vliw* vlp= new Vliw;

	execute(0x1007c, 0x00000793);
	execute(0x10080, 0x00078863);
	execute(0x10084, 0x00010537);
	execute(0x10088, 0x42850513);
	execute(0x1008c, 0x3880006f);
	execute(0x10090, 0x00008067);

	return 0;
}
*/

