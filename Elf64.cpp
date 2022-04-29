// Convert.cpp - ELF file to hex/bin converter
//  (C) Ron K. Irvine, 2005. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "RiscVFast.h"
#include "Cpu.h"
#include "Elf64.h"

// #include "elf.h"	- update sometime, maybe ;-)

#define BUF_SIZE (16*1024*1024)
/*Assumes running on PC little endian*/
#define ntohl(A) (littleEndian?(A):(((A)>>24)|(((A)&0x00ff0000)>>8)|(((A)&0xff00)<<8)|((A)<<24)))
#define ntohs(A) (littleEndian?(A):((((A)&0xff00)>>8)|((A)<<8)))
int littleEndian = 1;
bool addGpInfo = false;		// add 'gp' and stack info to the program

#if defined(ELF64)
// use 64 bit structs ...
#define Elf32_Ehdr Elf64_Ehdr
#define Elf32_Shdr Elf64_Shdr
#define Elf32_Phdr Elf64_Phdr
#define Elf32_Sym  Elf64_Sym
#endif

bool is_rom = true;			// pack for a ROM (.text + copy of .data)

bool do_hex = false;        // produce a hex file
bool do_bin = false;        // produce a binary file
bool do_mif = false;        // produce a mif file
bool do_mif_i = false;      // produce a mif file (iverilog format)
bool do_dinit = false;		// produce a data initialization file
bool do_byteswap = false;	// byte swap .hex/.bin files
unsigned do_ihexSize = 0x0000;	// max .text hex output size in bytes
unsigned do_dhexSize = 0x0000;	// max .data hex output size in bytes

bool do_id_space = false;			// separate output into Instruction/Data files

char* inputFilename = NULL;
char* baseFilename = NULL;

int verbose = 0;

// prototypes
void initData(unsigned char* data, unsigned start, unsigned end);

/*
void usage(void) {
	printf("Convert <options> file.elf\n");
	printf("  -hex      output a hex file; *.hex\n");
	printf("  -bin      output a binary file; *.bin\n");
	printf("  -mif      output a mif format file; *.mif\n");
	printf("  -mif_i    output a mif iverilog format file; *_i.mif\n");
	printf("  -dinit    output a data initialization file; data_init\n");
	printf("  -idspace  output a separate instruction/data space\n");
	printf("  -isize=xx  maximum .text size (hex number)\n");
	printf("  -dsize=xx  maximum .data size (hex number)\n");
}
*/

// Section Header Types to string
const char* sectHdr2str(int type) {
	if(type == SHT_PROGBITS) {
		return "SHT_PROGBITS";
	}
	if(type == SHT_NOBITS) {
		return "SHT_NOBITS";
	}
	if(type == SHT_PROGBITS) {
		return "SHT_PROGBITS";
	}
	if(type == SHT_INIT_ARRAY) {
		return "SHT_INIT_ARRAY";
	}
	if(type == SHT_FINI_ARRAY) {
		return "SHT_FINI_ARRAY";
	}
	return "None";
}

// Section flags to string
const char* flags2str(int flags) {
	static char buf[16];
	char* bp = buf;
	if(flags & SHF_WRITE) {
		*bp++ = 'W';
	}
	if(flags & SHF_ALLOC) {
		*bp++ = 'A';
	}
	if(flags & SHF_EXECINSTR) {
		*bp++ = 'X';
	}
	*bp = '\0';
	return buf;
};

unsigned hextoi(char* s) {
	unsigned ret = 0;
	sscanf(s, "%x", &ret);
	return ret;
};

void set_low(char* ptr,unsigned long address,unsigned long value) {
	unsigned long opcode;
	opcode=*(unsigned long*)(ptr+address);
	opcode=ntohl(opcode);
	opcode=(opcode&0xffff0000)|(value&0xffff);
	opcode=ntohl(opcode);
	*(unsigned long*)(ptr+address)=opcode;
}

//	outHexLine - output a line of Intel format hex
//  type:  0x00 - data, 0x01 - end record, 0x02 - segment
void outHexLine(FILE* fp, unsigned short add, unsigned char* data,
	unsigned len, unsigned type) {
	int checksum;

	fprintf(fp, ":%02X%04X%02X", len, add, type);

	checksum = len + add + (add>>8) + type;
	for(unsigned i = 0;  i < len;  ++i) {
		fprintf(fp, "%02X", data[i]&0xff);
		checksum += data[i];
	}
	fprintf(fp, "%02X\n", (-checksum)&0xff);
};

void outHexSegment(FILE* ofp, unsigned short segment) {
	unsigned char data[2];
	// segment >>= 4;
	data[0] = (char)(segment>>8);
	data[1] = (char)(segment);
	outHexLine(ofp, 0x0000, data, 2, 2);	// 20bit address segment
};

void saveIntel(char* filename, unsigned char* data, unsigned len) {
	unsigned short segment = 0xffff;
	unsigned long addr = 0x00000;
	int blockSize = 32;
	FILE* ofp;

	// check for boundary

	ofp = fopen(filename, "w");
	if(ofp == NULL) {
		printf("Could not open hex file: %s\n", filename);
		return;
	}

	for(unsigned n=0; n<len; n+=blockSize) {    // 32 bytes at a time
		int cnt;

		if((unsigned short)((addr>>4)&0xf000) != segment) {
			segment = (addr>>4)&0xf000;
			outHexSegment(ofp, segment);
		}
		cnt = len - n;
		if(cnt > blockSize) {
			cnt = blockSize;
		}

		outHexLine(ofp, addr&0xffff, data, cnt, 0x00);
		data += blockSize;
		addr += blockSize;
	}

	/* end the hex file */
	fprintf(ofp, ":00000001FF\n");
	fclose(ofp);
	printf("Write 0x%06x data bytes to %s\n", len, filename);
};

void saveBinary(char* filename, unsigned char* data, unsigned len) {
	FILE* outfile=fopen(filename, "wb");
	if(outfile == NULL) {
		return;
	}
	fwrite(data, len, 1, outfile);
	fclose(outfile);
	printf("Write 0x%06x data bytes to %s\n", len, filename);
};

void saveMif(char* filename, unsigned char* data, unsigned len) {
	FILE* txtfile=fopen(filename,"w");
	if(txtfile == NULL) {
		return;
	}

	unsigned width = 32;
	unsigned depth = ((len+3)/4);
	// printf("Mif file %s - %s, %d x %d (length = %d)\n", filename, inputFilename, width, depth, len);
	fprintf(txtfile, "%%Rom memory - %s, %d x %d%%\n", filename, width, depth);
	fprintf(txtfile, "\n");
	fprintf(txtfile, "DEPTH = %d;\n", depth);
	fprintf(txtfile, "WIDTH = %d;\n", width);
	fprintf(txtfile, "ADDRESS_RADIX = HEX;\n");
	fprintf(txtfile, "DATA_RADIX = HEX;\n");
	fprintf(txtfile, "CONTENT BEGIN\n");
	for(unsigned i=0; i<len; i+=4) {
		unsigned d=ntohl(*(unsigned long*)(data+i));
		fprintf(txtfile, " %04x : %8.8x;\n", i/4, d);
	}
	fprintf(txtfile, "END;\n");
	fclose(txtfile);
	printf("Write 0x%06x data bytes to %s\n", len, filename);
};

void saveMif_i(char* filename, unsigned char* data, unsigned len) {
	FILE* outfile = fopen(filename, "w");
	if(outfile == NULL) {
		return;
	}
	for(unsigned i=0; i<len; i+=4) {
		unsigned d=ntohl(*(unsigned long*)(data+i));
		fprintf(outfile, "%8.8x\n", d);
	}
	fclose(outfile);
	printf("Write 0x%06x data bytes to %s\n", len, filename);
};

void saveMif8_i(char* filename, unsigned char* data, unsigned len) {
	FILE* outfile = fopen(filename, "w");
	if(outfile == NULL) {
		return;
	}
	for(unsigned i=0; i<len; ++i) {
		fprintf(outfile, "%8.8x\n", data[i]);
	}
	fclose(outfile);
	printf("Write 0x%06x data bytes to %s\n", len, filename);
};

/*
unsigned char* loadSection(Elf32_Shdr* elfSection) {
      elfSection->sh_name=ntohl(elfSection->sh_name);
        elfSection->sh_type=ntohl(elfSection->sh_type);
        elfSection->sh_flags=ntohl(elfSection->sh_flags);
        elfSection->sh_addr=ntohl(elfSection->sh_addr);
        elfSection->sh_offset=ntohl(elfSection->sh_offset);
        elfSection->sh_size=ntohl(elfSection->sh_size);
	return NULL;
}
*/

// compare: ".text.startup" to ".text*"
bool sectCmp(const char* name, const char* sect) {
	const char* s = name;
	const char* t = sect;
	for(;;) {
		if(*s != *t) {
			if(*s == '.' && *t == '*') {
				return true;
			}
			return false;
		}
		if(*s == '\0') {
			break;
		}
		++s, ++t;
	}
	return true;
};

class Section {
public:
	const char*		name;
	unsigned char* 	buf;
	unsigned 		address;
	unsigned 		size;
	unsigned 		offset;

	Section(const char* n) {
		name = strdup(n);
		size = 0;
		address = 0;
		buf = NULL;
		offset = 0;
	};

	void dump(void) {
		printf("%-6.6s addr=%08x, size=%08x\n", name, address, size);
	};

};

void byteSwap(unsigned char* buf, unsigned size) {
	for(unsigned i=0; i<size; i+=4) {
		unsigned char t0 = buf[i];
		unsigned char t1 = buf[i+1];
		unsigned char t2 = buf[i+2];
		unsigned char t3 = buf[i+3];
		buf[i] = t3;
		buf[i+1] = t2;
		buf[i+2] = t1;
		buf[i+3] = t0;
	}
};

int readElf(const char* inputFilename, Memory* memory, Cpu* cpu,
	uint32_t heap_len, uint32_t* entry) {

	// FILE* infile, *outfile, *txtfile;
	FILE* infile;
	unsigned char* elfbuf;
	// printf("Convert: (C) Ron K. Irvine, 2020.\n");
	// printf("  Build: %s %s\n", __DATE__, __TIME__);

	// Section textSect(".text");
	// Section dataSect(".data");
	// Section bssSect(".bss");

	Segment* code = memory->insnMem;
	Segment* data = memory->dataMem;

	uint32_t size;
	// long stack_pointer;
	// unsigned long d;
	uint32_t gp_ptr=0;
	uint32_t seg_bss_start=0;
	uint32_t seg_bss_end=0;
	unsigned width, depth;
	int e_type;

	Elf32_Ehdr* elfHeader;
	Elf32_Phdr* elfProgram;
	Elf32_Shdr* elfSection;
	char* stringTable = NULL;
	char* nameTable = NULL;

	// use to avoid gcc warnings!!!
	(void)gp_ptr;	// Use it
	(void)seg_bss_end;	// Use it
	(void)width;	// Use it
	(void)depth;	// Use it

	infile=fopen(inputFilename, "rb");
	if(infile==NULL) {
		printf("Can't open %s\n", inputFilename);
		return -1;
	}

	// determine the size of the file
	fseek(infile, 0, SEEK_END);
	uint32_t filesize = ftell(infile);
	fseek(infile, 0, SEEK_SET);

	elfbuf = (unsigned char*)malloc(filesize);
	if(elfbuf == NULL) {
		printf("Can not allocate file buffer\n");
		return(-1);
	}

	// Read the entire ELF into the elfbuf
	size = fread(elfbuf, 1, filesize, infile);
	if(size != filesize) {
		printf("Can not read file into buffer, %d bytes\n", size);
		return(-1);
	}
	if(verbose) {
		printf("Input file: %s\n", inputFilename);
	}
	fclose(infile);
	// printf("elfbuf: @ %08x, len = %08x\n", elfbuf, size);

	// elfbuf now contains the entire elf file
	elfHeader=(Elf32_Ehdr*)elfbuf;
	if(strncmp((const char*)(&elfHeader->e_ident[1]), "ELF", 3)) {
		printf("Error:  Not an ELF file!\n");
		// printf("Use the gccmips_elf.zip from opencores/projects/plasma!\n");
		return -1;
	}
	// determine endianness
	e_type = elfHeader->e_ident[5];

	if(verbose) {
		// printf("E_Type = %x\n", e_type);
		if(e_type == 2) {	// is it little endian?
			littleEndian = 0;
			printf("Big endian\n");
		}
		else {
			littleEndian = 1;
			printf("Little endian\n");
		}
		switch(elfHeader->e_ident[4]) {
		case 1: printf("Elf Class: ELFCLASS32\n"); break;
		case 2: printf("Elf Class: ELFCLASS64\n"); break;
		default: printf("Elf Class: ELFCLASSNONE\n"); break;
		}
		printf("EI_VERSION: %d, EI_OSABI: %d, EI_ABIVERSION: %d\n",
			elfHeader->e_ident[6], elfHeader->e_ident[7], elfHeader->e_ident[8]);
		printf("String table section: %d\n", elfHeader->e_shstrndx);
	}

#if ELF64
	if(elfHeader->e_ident[4] != 2) {
		printf("Must be ELFCLASS64 - 64 bit Elf\n");
		exit(-1);
	}
#else
	if(elfHeader->e_ident[4] != 1) {
		printf("Must be ELFCLASS32 - 32 bit Elf\n");
		exit(-1);
	}
#endif

	elfHeader->e_machine=ntohl(elfHeader->e_machine);
#define EM_MIPS 8
#define EM_RISCV 243
	// MIPS/RISC-V support only
	int machine = elfHeader->e_machine;
	if(machine != 0 && machine != EM_MIPS && machine != 0 && machine != EM_RISCV) {
		printf("*** Error *** not a supported machine type: %d\n", elfHeader->e_machine);
		return -1;
	}
	if(!quiet) {
		if(machine == EM_MIPS) {
			printf("Machine: MIPS\n");
		}
		if(machine == EM_RISCV) {
			printf("Machine: RISC-V\n");
		}
	}

	elfHeader->e_entry=ntohl(elfHeader->e_entry);
	elfHeader->e_phoff=ntohl(elfHeader->e_phoff);
	elfHeader->e_shoff=ntohl(elfHeader->e_shoff);
	elfHeader->e_phentsize=ntohs(elfHeader->e_phentsize);
	elfHeader->e_phnum=ntohs(elfHeader->e_phnum);
	elfHeader->e_shentsize=ntohs(elfHeader->e_shentsize);
	elfHeader->e_shnum=ntohs(elfHeader->e_shnum);

	unsigned long long entryPoint = elfHeader->e_entry;
	if(verbose) {
		printf("Entry: 0x%08llx\n", entryPoint);
	}
	*entry = entryPoint;

	for(int i=0; i<elfHeader->e_phnum; ++i) {
		elfProgram=(Elf32_Phdr*)(elfbuf+elfHeader->e_phoff+elfHeader->e_phentsize*i);
		elfProgram->p_offset=ntohl(elfProgram->p_offset);
		elfProgram->p_vaddr=ntohl(elfProgram->p_vaddr);
		elfProgram->p_filesz=ntohl(elfProgram->p_filesz);
		elfProgram->p_memsz=ntohl(elfProgram->p_memsz);
		// printf("[0x%x, 0x%x, 0x%x]\n",elfProgram->p_vaddr,elfProgram->p_offset,elfProgram->p_filesz);

		if(verbose >= 1) {
#if ELF64
			printf("Segment [vaddr:%08llx, off:%08llx, fsize:%08llx, msize:%08llx, flags:%08x]\n",
				elfProgram->p_vaddr, elfProgram->p_offset, elfProgram->p_filesz,
				elfProgram->p_memsz, elfProgram->p_flags);
#else
			printf("Segment [vaddr:%08x, off:%08x, fsize:%08x, msize:%08x, flags:%08x]\n",
				elfProgram->p_vaddr, elfProgram->p_offset, elfProgram->p_filesz,
				elfProgram->p_memsz, elfProgram->p_flags);
#endif
		}
	}

	// find the name table
	for(int i=0; i<elfHeader->e_shnum; ++i) {
		elfSection=(Elf32_Shdr*)(elfbuf+elfHeader->e_shoff+elfHeader->e_shentsize*i);
		elfSection->sh_name=ntohl(elfSection->sh_name);
		elfSection->sh_type=ntohl(elfSection->sh_type);
		elfSection->sh_flags=ntohl(elfSection->sh_flags);
		elfSection->sh_addr=ntohl(elfSection->sh_addr);
		elfSection->sh_offset=ntohl(elfSection->sh_offset);
		elfSection->sh_size=ntohl(elfSection->sh_size);

		if(verbose >= 2) {
#if ELF64
			printf("[%2d] {name:0x%x, type:0x%x, flags:0x%0llx, addr:0x%llx, off:0x%llx, size:0x%llx}\n",
				i, elfSection->sh_name, elfSection->sh_type, elfSection->sh_flags,
				elfSection->sh_addr, elfSection->sh_offset, elfSection->sh_size);
#else
			printf("[%2d] {name:0x%x, type:0x%x, flags:0x%0x, addr:0x%x, off:0x%x, size:0x%x}\n",
				i, elfSection->sh_name, elfSection->sh_type, elfSection->sh_flags,
				elfSection->sh_addr, elfSection->sh_offset, elfSection->sh_size);
#endif
		}

		// Name table
		if(nameTable == NULL && elfSection->sh_type == 3 && i == elfHeader->e_shstrndx) {
			nameTable = (char*)&elfbuf[elfSection->sh_offset];
			if(verbose >= 1) {
#if ELF64
				printf("[%2d] Name table @ 0x%08llx\n", i, elfSection->sh_offset);
#else
				printf("[%2d] Name table @ 0x%08x\n", i, elfSection->sh_offset);
#endif
			}
		}
		// String table
		if(stringTable == NULL && elfSection->sh_type == 3 && i != elfHeader->e_shstrndx) {
			stringTable = (char*)&elfbuf[elfSection->sh_offset];
			if(verbose >= 1) {
#if ELF64
				printf("[%2d] String table @ 0x%08llx\n", i, elfSection->sh_offset);
#else
				printf("[%2d] String table @ 0x%08x\n", i, elfSection->sh_offset);
#endif
			}
		}
	}

	for(int i=0; i<elfHeader->e_shnum; ++i) {
		elfSection=(Elf32_Shdr*)(elfbuf+elfHeader->e_shoff+elfHeader->e_shentsize*i);

#if ELF64
		if(verbose >= 2) {
			printf("[%2d] {%-16.16s: name:%08x, type:%08x, flags:%08llx, addr:%08llx, off:%08llx, size:%08llx}\n",
				i, &nameTable[elfSection->sh_name], elfSection->sh_name, elfSection->sh_type,
				elfSection->sh_flags, elfSection->sh_addr, elfSection->sh_offset, elfSection->sh_size);
		}
#else
		if(verbose >= 2) {
			printf("[%2d] {%-16.16s: name:%08x, type:%08x, flags:%08x, addr:%08x, off:%08x, size:%08x}\n",
				i, &nameTable[elfSection->sh_name], elfSection->sh_name, elfSection->sh_type,
				elfSection->sh_flags, elfSection->sh_addr, elfSection->sh_offset, elfSection->sh_size);
		}
#endif

		if(elfSection->sh_type==SHT_PROGBITS) {
			gp_ptr=elfSection->sh_addr;
		}
		if(elfSection->sh_type==SHT_NOBITS) {
			if(seg_bss_start==0) {
				seg_bss_start=elfSection->sh_addr;
			}
			seg_bss_end=elfSection->sh_addr+elfSection->sh_size;
		}

	}

	// Determine text/data areas
	uint32_t text_base = -1;
	uint32_t text_end = 0;
	uint32_t data_base = -1;
	uint32_t data_end = 0;
	uint32_t bss_base = -1;
	uint32_t bss_end = 0;
	// bool do_unified = false;
	uint32_t page_size = 0x1000;	// 4K pages ...

	for(int i=0; i<elfHeader->e_shnum; ++i) {
		elfSection=(Elf32_Shdr*)(elfbuf+elfHeader->e_shoff+elfHeader->e_shentsize*i);
		const char* name = &nameTable[elfSection->sh_name];
		uint32_t type = (uint32_t)(elfSection->sh_type);
		uint32_t flags = (uint32_t)(elfSection->sh_flags);
		uint32_t addr = (uint32_t)(elfSection->sh_addr);
		uint32_t len = elfSection->sh_size;
		// uint8_t* offset = elfbuf+elfSection->sh_offset;

		// comments, symbol tables, ...
		if(flags == 0x00 || flags == 0x30) {
			continue;
		}

		if(!quiet) {
			printf("[%02d] %-12.12s 0x%08x 0x%08x  [%s, %s]\n", i, name,
				addr, len, sectHdr2str(type), flags2str(flags));
		}

		if(strncmp(name, ".text", 5)==0 && (flags & (SHF_ALLOC|SHF_EXECINSTR))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len > text_end) {
				text_end = addr+len;
			}
		}
		else if(strncmp(name, ".rodata", 7)==0 && (flags & (SHF_ALLOC))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len > text_end) {
				text_end = addr+len;
			}
		}
		else if(strcmp(name, ".eh_frame")==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len > text_end) {
				text_end = addr+len;
			}
		}
		else if(strcmp(name, ".init_array")==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len > text_end) {
				text_end = addr+len;
			}
		}
		else if(strcmp(name, ".fini_array")==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len > text_end) {
				text_end = addr+len;
			}
		}
		else if(strcmp(name, ".eh_frame")==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Text\n");
			if(addr < text_base) {
				text_base = addr;
			}
			if(addr+len < text_end) {
				text_end = addr+len;
			}
		}
		else if(strncmp(name, ".data", 5)==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Data\n");
			if(addr < data_base) {
				data_base = addr;
			}
			if(addr+len > data_end) {
				data_end = addr+len;
			}
		}
		else if(strncmp(name, ".sdata", 6)==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Data\n");
			if(addr < data_base) {
				data_base = addr;
			}
			if(addr+len > data_end) {
				data_end = addr+len;
			}
		}
		else if(strncmp(name, ".bss", 4)==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Bss\n");
			if(addr < bss_base) {
				bss_base = addr;
			}
			if(addr+len > bss_end) {
				bss_end = addr+len;
			}
		}
		else if(strncmp(name, ".sbss", 5)==0 && (flags & (SHF_ALLOC|SHF_WRITE))) {
			// printf("  -> .Bss\n");
			if(addr < bss_base) {
				bss_base = addr;
			}
			if(addr+len > bss_end) {
				bss_end = addr+len;
			}
		}
		else {
			printf("Unknow program section: %s\n", name);
			exit(1);
		}
	}

	uint32_t text_len = text_end - text_base;
	uint32_t data_len = data_end - data_base;
	uint32_t bss_len = bss_end - bss_base;
	if(!quiet) {
		printf("text_base = 0x%08x, text_end = 0x%08x, text_len = 0x%08x\n",
			text_base, text_end, text_len);
		printf("data_base = 0x%08x, data_end = 0x%08x, data_len = 0x%08x\n",
			data_base, data_end, data_len);
		printf("bss_base  = 0x%08x, bss_end  = 0x%08x, bss_len  = 0x%08x\n",
			bss_base, bss_end, bss_len);
	}

	uint32_t heap_base = (bss_end + 15) & ~0xf;	// align to 16 bytes
	uint32_t heap_end = heap_base + heap_len;
	uint32_t total_len = heap_end - data_base;
	if(verbose) {
		printf("heap_base = 0x%08x, heap_end  = 0x%08x, heap_len  = 0x%08x\n",
			heap_base, heap_end, heap_len);
		printf("total_len = 0x%08x\n", total_len);
	}

	// Check sections are within a 4K page
	if(!quiet) {
		if((data_base-text_end) > page_size) {
			printf("  Separate: .text (0x%08x) and .data (0x%08x)\n", text_base, data_base);
		}
		else {
			printf("  Unified: .text and .data\n");
			// do_unified = true;
		}
	}

	// unified is .text == .data

	// setup new memory ...
	code->memBase = text_base;
	code->memLen = text_len;
	code->data = (uint8_t*)malloc(text_len);
	data->memBase = data_base;
	data->memLen = total_len;
	data->data = (uint8_t*)malloc(total_len);
	if(!quiet) {
		code->info();
		data->info();
	}

	if(0) {
		printf("===========================\n");
		code->info();
		data->info();
		printf("===========================\n");
	}

	// Load the memory sections
	// #define SHF_WRITE	     (1 << 0)
	// #define SHF_ALLOC	     (1 << 1)
	// #define SHF_EXECINSTR	 (1 << 2)
	// #define SHF_MERGE	     (1 << 4)
	// #define SHF_STRINGS	     (1 << 5)
	for(int i=0; i<elfHeader->e_shnum; ++i) {
		elfSection=(Elf32_Shdr*)(elfbuf+elfHeader->e_shoff+elfHeader->e_shentsize*i);
		const char* name = &nameTable[elfSection->sh_name];
		uint32_t type = (uint32_t)(elfSection->sh_type);
		uint32_t flags = (uint32_t)(elfSection->sh_flags);
		uint32_t addr = (uint32_t)(elfSection->sh_addr);
		uint32_t len = elfSection->sh_size;

		// offset is the file offset into the elfbuf
		uint8_t* offset = elfbuf+elfSection->sh_offset;

		// comments, symbol tables, ...
		if(flags == 0x00 || flags == 0x30) {
			continue;
		}

		bool textOk = (addr >= text_base) && (addr < text_end);
		bool dataOk = (addr >= data_base) && (addr < data_end);
		bool bssOk  = (addr >= bss_base)  && (addr < bss_end);

		bool alloc = (flags&SHF_ALLOC);
		// bool inst = (flags&SHF_EXECINSTR);
		bool rw = (flags&SHF_WRITE);
		bool nobits = (type == SHT_NOBITS);
		// bool progbits = ((type == SHT_PROGBITS) || type == SHT_INIT_ARRAY
		//		|| type == SHT_FINI_ARRAY);
		// bool read_only = progbits && !(flags&SHF_WRITE);

		// .text - Instructions and Read Only data
		if(textOk && (alloc /* && !rw*/)) {
			if(verbose) {
				printf(".text => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memcpy(code->data+(addr-code->memBase), offset, len);
			if(verbose) {
				printf("  memcpy - .text: 0x%08x, 0x%08x [0x%08x, 0x%08lx]\n", addr, len,
					(addr-code->memBase), offset-elfbuf);
			}

			Elf32_Info* ip = new Elf32_Info;
			ip->name = strdup(name);	// name of the section
			ip->type = type;			// member category
			ip->flags = flags;			// attribute  flags
			ip->addr = addr;			// physical address in bytes
			ip->size = len;				// section size in bytes
			ip->offset = elfSection->sh_offset;	// file offset in bytes
			// append to the memory section
			for(Elf32_Info** ipp=&(code->elf32); ; ipp = &((*ipp)->next)) {
				if((*ipp) == NULL) {
					ip->next = NULL;
					(*ipp) = ip;
					break;
				}
			}

		}
		// .data - Data Section
		else if(dataOk && (alloc && rw) && !nobits) {
			if(verbose) {
				printf(".data => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memcpy(data->data+(addr-data->memBase), offset, len);
			if(verbose) {
				printf("  memcpy - .data: 0x%08x, 0x%08x\n", addr, len);
			}

			Elf32_Info* ip = new Elf32_Info;
			ip->name = strdup(name);	// name of the section
			ip->type = type;			// member category
			ip->flags = flags;			// attribute  flags
			ip->addr = addr;			// physical address in bytes
			ip->size = len;				// section size in bytes
			ip->offset = elfSection->sh_offset;	// file offset in bytes
			// append to the memory section
			for(Elf32_Info** ipp=&(data->elf32); ; ipp = &((*ipp)->next)) {
				if((*ipp) == NULL) {
					ip->next = NULL;
					(*ipp) = ip;
					break;
				}
			}
		}
		// .bss - bss Section
		// else if(dataOk && (alloc && rw) && nobits) {
		else if(bssOk && (alloc && rw) && nobits) {
			if(verbose) {
				printf(".bss  => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memset(data->data+(addr-data->memBase), 0, len);
			if(verbose) {
				printf("  memzero - .bss: 0x%08x, 0x%08x\n", addr, len);
			}

			Elf32_Info* ip = new Elf32_Info;
			ip->name = strdup(name);	// name of the section
			ip->type = type;			// member category
			ip->flags = flags;			// attribute  flags
			ip->addr = addr;			// physical address in bytes
			ip->size = len;				// section size in bytes
			ip->offset = elfSection->sh_offset;	// file offset in bytes
			// append to the memory section
			for(Elf32_Info** ipp=&(data->elf32); ; ipp = &((*ipp)->next)) {
				if((*ipp) == NULL) {
					ip->next = NULL;
					(*ipp) = ip;
					break;
				}
			}
		}
		else {
			printf(".???? => %-12.12s 0x%08x  [%02x, %02x]\n", name, addr, type, flags);
		}
	}

	if(elfbuf) {
		free(elfbuf);
	}

	// getchar();

	return 0;
}

// initData - create code to initialize the data section
void initData(unsigned char* data, unsigned start, unsigned end) {
	FILE* fp = fopen("InitData.s", "w");
	if(fp == NULL) {
		return;
	}

	// simple brute force technique
	fprintf(fp, "\t# data initialization\n");
	fprintf(fp, "\n");

	fprintf(fp, "\t.set nomips16\n");
	// fprintf(fp, "#include \"regs.s\"\n");
	fprintf(fp, "\n");

	fprintf(fp, "\t.text\n");
	fprintf(fp, "\t.glob\t_initData\n");
	fprintf(fp, "\t.ent\t_initData\n");
	fprintf(fp, "\n");

	fprintf(fp, "_initData:\n");

	// zero the data section (if .bss is not done here it should be done elsewhere)
	// printf("\t# zero data section\n");
	fprintf(fp, "\tla $2,0x%x\n", start);
	fprintf(fp, "\tla $3,0x%x\n", end);
	fprintf(fp, "3:\tsw $0,0($2)\n");
	fprintf(fp, "\tbltu $2,$3,3b\n");
	fprintf(fp, "\taddiu $2,$2,4\n");
	fprintf(fp, "\n");

	// set up a base pointer
	fprintf(fp, "\t# Set up base pointer and copy data\n");
	fprintf(fp, "\tla $2,%d\n", start);		// use $2 for a base pointer

	// unsigned* dp = (unsigned*)data;
	for(unsigned i=start; i<end; i += 4) {
		unsigned d;
		if(littleEndian) {
			d = (data[i+3]<<24) | (data[i+2]<<16) | (data[i+1]<<8) | data[i];
		}
		else {
			d = (data[i]<<24) | (data[i+1]<<16) | (data[i+2]<<8) | data[i+3];
		}
		if(d != 0) {
			fprintf(fp, "\tla $3,0x%x\n", d);
			fprintf(fp, "\tsw $3,0x%x($2)\n", i-start);
		}
	}
	fprintf(fp, "\tjr $31\n");
	fprintf(fp, "\tnop\n");
	fprintf(fp, "\t.end\t_initData\n");
	fprintf(fp, "\n");
};

