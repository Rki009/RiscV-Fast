// Convert.cpp - ELF file to hex/bin converter
//  (C) Ron K. Irvine, 2005. All rights reserved.


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "RiscVFast.h"
#include "Elf64.h"

// #include "elf.h"	- update sometime, maybe ;-)

#define BUF_SIZE (16*1024*1024)
/*Assumes running on PC little endian*/
#define ntohl(A) (littleEndian?(A):(((A)>>24)|(((A)&0x00ff0000)>>8)|(((A)&0xff00)<<8)|((A)<<24)))
#define ntohs(A) (littleEndian?(A):((((A)&0xff00)>>8)|((A)<<8)))
int littleEndian = 1;
bool addGpInfo = false;		// add 'gp' and stack info to the program


#if 0
#define EI_NIDENT 16
#define SHT_PROGBITS 1
#define SHT_STRTAB 3
#define SHT_NOBITS 8
#pragma pack(1)
// Header
typedef struct {
	unsigned char  e_ident[EI_NIDENT];  // ident bytes
	unsigned short e_type;				// file type
	unsigned short e_machine;           // target machine
	unsigned long  e_version;           // file version
	unsigned long  e_entry;             // start address
	unsigned long  e_phoff;             // phdr file offset
	unsigned long  e_shoff;             // shdr file offset
	unsigned long  e_flags;             // file flags
	unsigned short e_ehsize;            // size of ehdr
	unsigned short e_phentsize;         // size of phdr
	unsigned short e_phnum;             // number of phdrs
	unsigned short e_shentsize;         // size of shdrs
	unsigned short e_shnum;             // number of shdrs
	unsigned short e_shstrndx;          // shdr string index
} ElfHeader;

// Segment
typedef struct {
	unsigned long p_type;		// entry type
	unsigned long p_offset;      // file offset
	unsigned long p_vaddr;       // virtual address
	unsigned long p_paddr;       // physical address
	unsigned long p_filesz;      // file size
	unsigned long p_memsz;       // memory size
	unsigned long p_flags;       // flags
	unsigned long p_align;       // memory/file alignment
} Elf32_Phdr;

// Section
typedef struct {
	unsigned long sh_name;		// section name - index into name table
	unsigned long sh_type;      // SHT_... type
	unsigned long sh_flags;		// SHF_... flags
	unsigned long sh_addr;      // virtual address
	unsigned long sh_offset;    // file offset
	unsigned long sh_size;      // section size
	unsigned long sh_link;      // misc info
	unsigned long sh_info;      // misc info
	unsigned long sh_addralign; // memory alignment
	unsigned long sh_entsize;   // entry size if table
} Elf32_Shdr;
#pragma pack()

// Section Header Types
#define SHT_NULL		0		// header is inactive - ignore
#define SHT_PROGBITS	1		// hold program information (.text, .data)
#define SHT_SYMTAB		2		// symbol table
#define SHT_STRTAB		3		// string table
#define SHT_RELA		4		// relocation entries
#define SHT_HASH		5		// symbol hash table
#define SHT_DYNAMIC		6		// dynamic linking info
#define SHT_NOTE		7		// file notes
#define SHT_NOBITS		8		// program information, no data (.bss)
#define SHT_REL			9		// relocation entries
#define SHT_SHLIB		10		// reserved
#define SHT_DYNSYM		11		//
#define SHT_LOPROC		0x70000000	// reserved for processor specific info
#define SHT_HIPROC		0x7fffffff	// ...
#define SHT_LOUSER		0x80000000	// reserved for application specific info
#define SHT_HIUSER		0xffffffff	// ...

// Section flags
#define SHF_WRITE		0x01		// writeable during execution
#define SHF_ALLOC		0x02		// memory during execution (.bss)
#define SHF_EXECINSTR	0x04		// instructions
#define SHF_MASK		0x07		// flags mask
#define SHF_RONLY		0x10		// ?? read only
#define SHF_OONLY2		0x20		// ??
#define SHF_MASKPROC	0xf0000000	// processor specific bits

// flag value for "special" sections
#define	SHF_TEXT	(SHF_ALLOC | SHF_EXECINSTR)
#define	SHF_BSS		(SHF_ALLOC | SHF_WRITE)
#define	SHF_DATA	(SHF_ALLOC | SHF_WRITE)
#define	SHF_RODATA	(SHF_ALLOC)

#define PF_X	0x01		// execute only
#define PF_W	0x02        // write only
#define PF_R	0x04        // read only
#endif


#if defined(ELF64)
// use 64 bit structs ...
#define Elf32_Ehdr Elf64_Ehdr
#define Elf32_Shdr Elf64_Shdr
#define Elf32_Phdr Elf64_Phdr
#define Elf32_Sym  Elf64_Sym
#endif



#if 0
unsigned long load(unsigned char* ptr,unsigned long address) {
	unsigned long value;
	value=*(unsigned long*)(ptr+address);
	value=ntohl(value);
	return value;
}

unsigned short load_short(unsigned char* ptr,unsigned long address) {
	return (ptr[address]<<8)+ptr[address+1];
}
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
#if 0
	if(len%blockSize != 0) {
		printf("saveIntel(): invalid length %d\n", len);
		return;
	}
#endif

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

int readElf(const char* inputFilename, Segment* code, Segment* data, uint32_t* entry) {
	// FILE* infile, *outfile, *txtfile;
	FILE* infile;
	unsigned char* elfbuf;
	// printf("Convert: (C) Ron K. Irvine, 2020.\n");
	// printf("  Build: %s %s\n", __DATE__, __TIME__);

	// Section textSect(".text");
	// Section dataSect(".data");
	// Section bssSect(".bss");

	uint32_t size;
	// long stack_pointer;
	// unsigned long d;
#if 1
	uint32_t gp_ptr=0;
#endif
	uint32_t bss_start=0;
	uint32_t bss_end=0;
	unsigned width, depth;
	int e_type;

	Elf32_Ehdr* elfHeader;
	Elf32_Phdr* elfProgram;
	Elf32_Shdr* elfSection;
	char* stringTable = NULL;
	char* nameTable = NULL;


	// use to avoid gcc warnings!!!
	(void)gp_ptr;	// Use it
	(void)bss_end;	// Use it
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
#if 0
		// only copy executable data into code section
		if(elfProgram->p_flags == (PF_X | PF_R)) {	// rx - flags
			if(elfProgram->p_vaddr+elfProgram->p_filesz >= BUF_SIZE) {
				printf("Program size too large: [0x%x, 0x%x, 0x%x]\n", elfProgram->p_vaddr, elfProgram->p_offset, elfProgram->p_filesz);
				exit(-1);
			}
			memcpy(code+elfProgram->p_vaddr, elfbuf+elfProgram->p_offset, elfProgram->p_filesz);
			length=elfProgram->p_vaddr + elfProgram->p_memsz;
		}
#endif
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

#if 0	// converted above in search for name table; rki - May08
		elfSection->sh_name=ntohl(elfSection->sh_name);
		elfSection->sh_type=ntohl(elfSection->sh_type);
		elfSection->sh_addr=ntohl(elfSection->sh_addr);
		elfSection->sh_offset=ntohl(elfSection->sh_offset);
		elfSection->sh_size=ntohl(elfSection->sh_size);
#endif

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

#if 0
		if(elfSection->sh_type==SHT_PROGBITS||elfSection->sh_type==SHT_STRTAB) {
			//         memcpy(code+elfSection->sh_addr,elfbuf+elfSection->sh_offset,elfSection->sh_size);
			length=elfSection->sh_addr+elfSection->sh_size;
			bss_start=length;
		}
#endif
#if 1
		if(elfSection->sh_type==SHT_PROGBITS) {
			gp_ptr=elfSection->sh_addr;
		}
#endif
		if(elfSection->sh_type==SHT_NOBITS) {
			if(bss_start==0) {
				bss_start=elfSection->sh_addr;
			}
			bss_end=elfSection->sh_addr+elfSection->sh_size;
		}

#if 0
		char* sectName = &nameTable[elfSection->sh_name];
		if(strcmp(sectName, ".text")==0) {
			textSect.size = elfSection->sh_size;
			textSect.offset = elfSection->sh_offset;
			textSect.buf = elfbuf+elfSection->sh_offset;
			textSect.address = elfSection->sh_addr;
		}
		else if(strcmp(sectName, ".data")==0) {
			dataSect.size = elfSection->sh_size;
			dataSect.offset = elfSection->sh_offset;
			dataSect.buf = elfbuf+elfSection->sh_offset;
			dataSect.address = elfSection->sh_addr;
		}
		else if(strcmp(sectName, ".bss")==0) {
			bssSect.size = elfSection->sh_size;
			bssSect.offset = elfSection->sh_offset;
			bssSect.buf = elfbuf+elfSection->sh_offset;
			bssSect.address = elfSection->sh_addr;
		}
#endif
	}

#if 0
	textSect.dump();
	dataSect.dump();
	bssSect.dump();
#endif

#if 0
	// add the data to the end of the code section - to copy on startup
	if(!do_id_space) {
		unsigned len = textSect.size + dataSect.size;
		unsigned char* textbuf = (unsigned char*)malloc(len);
		memcpy(textbuf, textSect.buf, textSect.size);
		memcpy(&textbuf[textSect.size], dataSect.buf, dataSect.size);
		textSect.size = len;
		textSect.buf = textbuf;
		printf("Append .data to .text\n");
		textSect.dump();
	}
#endif


#if 0
	// Load section into memory
	for(int i=0; i<elfHeader->e_shnum; ++i) {
		elfSection=(Elf32_Shdr*)(elfbuf+elfHeader->e_shoff+elfHeader->e_shentsize*i);
		const char* name = &nameTable[elfSection->sh_name];
		uint32_t type = (uint32_t)(elfSection->sh_type);
		uint32_t flags = (uint32_t)(elfSection->sh_flags);

		//Load Code
		// if((flags & (SHF_ALLOC | SHF_EXECINSTR))!=0 || strcmp(name, ".text")==0 || strcmp(name, ".rodata")==0 ) {
		// if(	(type == 1 && (flags == 0x6 || flags == 0x2)) || ((type == 0xe || type== 0xf) && flags&0x3) ||
		// if(	(type == 1 && (flags == 0x6 || flags == 0x2)) || ((type == 0xe || type== 0xf) && flags&0x3) ||
		if(sectCmp(name, ".text*")==0 || sectCmp(name, ".rodata*")==0 || sectCmp(name, ".init_array")==0
			|| sectCmp(name, ".fini_array")==0) {
			printf(".text = 0x%08x, CodeLen = 0x%08x\n", code->memBase, code->memLen);
			uint32_t addr = elfSection->sh_addr - code->memBase;
			uint32_t len = elfSection->sh_size;
			printf("  Section: %12.12s - addr=0x%08x, len=0x%08x\n", name, addr, len);

			if(addr >= code->memLen || (addr+len) >= code->memLen) {
				printf("  Section: %12.12s - out of range, addr=0x%08x, len=0x%08x\n", name, addr, len);
			}
			else {
				memcpy(code->data+addr, elfbuf+elfSection->sh_offset, elfSection->sh_size);
				printf("  Load %12.12s addr=0x%08x, [0x%08x], len=0x%08x\n", name,
					(uint32_t)(elfSection->sh_addr), addr, len);
			}
		}
		// Load Data
		// else if( (type == 1 && flags&0x3) ||
		else if(sectCmp(name, ".data*") ==0 || sectCmp(name, ".sdata*") ==0) {
			printf(".data = 0x%08x, DataLen = 0x%08x\n", data->memBase, data->memLen);
			uint32_t addr = elfSection->sh_addr - data->memBase;
			uint32_t len = elfSection->sh_size;

			if(addr >= data->memLen || (addr+len) >= data->memLen) {
				printf("  Section: %12.12s - out of range, addr=0x%08x, len=0x%08x\n", name, addr, len);
			}
			else {
				memcpy(data->data+addr, elfbuf+elfSection->sh_offset, elfSection->sh_size);
				printf("  Load %12.12s addr=0x%08x, [0x%08x], len=0x%08x\n", name,
					(uint32_t)(elfSection->sh_addr), addr, len);
			}
		}
		// Clear Bss
		// else if( (type == 8 && (flags == 0x3)) ||
		else if(sectCmp(name, ".bss*") ==0 || sectCmp(name, ".sbss*") ==0) {
			printf(".bss = 0x%08x, DataLen = 0x%08x\n", data->memBase, data->memLen);
			uint32_t addr = elfSection->sh_addr - data->memBase;
			uint32_t len = elfSection->sh_size;

			if(addr >= data->memLen || (addr+len) >= data->memLen) {
				printf("  Section:%12.12s - out of range, addr=0x%08x, len=0x%08x\n", name, addr, len);
			}
			else {
				memset(data->data+addr, 0, elfSection->sh_size);
				printf("  Load %12.12s addr=0x%08x, [0x%08x], len=0x%08x\n", name,
					(uint32_t)(elfSection->sh_addr), addr, len);
			}
		}
	}
#endif


#if 1
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
		uint8_t* offset = elfbuf+elfSection->sh_offset;

		// comments, symbol tables, ...
		if(flags == 0x00 || flags == 0x30) {
			continue;
		}

#if 0
		bool textOk = code->ok(addr) && (type == SHT_PROGBITS || type == SHT_INIT_ARRAY
				|| type == SHT_FINI_ARRAY);
		bool dataOk = data->ok(addr) && (type == SHT_PROGBITS);
		bool bssOk = data->ok(addr) && (type == SHT_NOBITS);
#endif
#if 1
		bool textOk = code->ok(addr);
		bool dataOk = data->ok(addr);
		// bool bssOk = data->ok(addr);
#endif

		bool alloc = (flags&SHF_ALLOC);
		// bool inst = (flags&SHF_EXECINSTR);
		bool rw = (flags&SHF_WRITE);

		// .text - Instrustion and Read Only data
		if(textOk && (alloc /* && !rw*/)) {
			if(!quiet) {
				printf(".text => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memcpy(code->data+(addr-code->memBase), offset, len);
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
		else if(dataOk && (alloc && rw)) {
			if(!quiet) {
				printf(".data => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memcpy(data->data+(addr-data->memBase), offset, len);
			Elf32_Info* ip = new Elf32_Info;
			ip->name = strdup(name);	// name of the section
			ip->type = type;			// member category
			ip->flags = flags;			// attribute  flags
			ip->addr = addr;			// physical address in bytes
			ip->size = len;			// section size in bytes
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
		else if(dataOk && (!alloc && rw)) {
			if(!quiet) {
				printf(".bss  => %-12.12s 0x%08x 0x%08x  [%02x, %02x]\n", name, addr, len, type, flags);
			}
			memset(data->data+(addr-data->memBase), 0, len);
			Elf32_Info* ip = new Elf32_Info;
			ip->type = type;			// member category
			ip->flags = flags;			// attribute  flags
			ip->addr = addr;			// physical address in bytes
			ip->size = len;			// section size in bytes
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
#endif


#if 0
	if(addGpInfo) {
		if(bss_start == codeLen) {
			bss_start = codeLen;
			bss_end = codeLen+4;
		}

		/*Initialize the $gp register for sdata and sbss*/
		gp_ptr+=0x7ff0;
		printf("gp_ptr=0x%x ",gp_ptr);
		/*modify the first opcodes in boot.asm*/
		/*modify the lui opcode*/
		set_low(code, 0, gp_ptr>>16);
		/*modify the ori opcode*/
		set_low(code, 4, gp_ptr&0xffff);

		/*Clear .sbss and .bss*/
		printf(".sbss=0x%x .bss_end=0x%x\n",bss_start,bss_end);
		set_low(code, 8, bss_start>>16);
		set_low(code, 12, bss_start&0xffff);
		set_low(code, 16, bss_end>>16);
		set_low(code, 20, bss_end&0xffff);

		/*Set stack pointer*/
		stack_pointer = bss_end+512;
		printf("Stack pointer=0x%x\n",stack_pointer);
		set_low(code, 24, stack_pointer>>16);
		set_low(code, 28, stack_pointer&0xffff);
	}
#endif

#if 0
	// output code
	if(do_hex) {
		char outFilename[256];
		sprintf(outFilename, "%s.hex", baseFilename);

		unsigned outSize = textSect.size;
		if(do_ihexSize!= 0) {
			if(do_ihexSize >= outSize) {
				outSize = do_ihexSize;
			}
			else {
				printf("*** WARNING *** - ihexSize: 0x%0x > 0x%0x\n",
					textSect.size, do_ihexSize);
			}
		}
		char* buf = (char*)calloc(outSize, 1);
		memcpy(buf, textSect.buf, textSect.size);
		if(do_byteswap) {
			byteSwap(buf, textSect.size);

		}
		saveIntel(outFilename, buf, outSize);
		free(buf);
	}
	if(do_bin) {
		char outFilename[256];
		sprintf(outFilename, "%s.bin", baseFilename);
		if(do_byteswap) {
			char* buf = (char*)malloc(textSect.size);
			memcpy(buf, textSect.buf, textSect.size);
			byteSwap(buf, textSect.size);

			saveBinary(outFilename, buf, textSect.size);
			free(buf);
		}
		else {
			saveBinary(outFilename, textSect.buf, textSect.size);
		}
	}
	if(do_mif) {
		char outFilename[256];
		sprintf(outFilename, "%s.mif", baseFilename);
		saveMif(outFilename, textSect.buf, textSect.size);
	}

	if(do_mif_i) {
		char outFilename[256];
		sprintf(outFilename, "%s_i.mif", baseFilename);
		saveMif_i(outFilename, textSect.buf, textSect.size);
	}

	if(do_id_space) {
		// output code
		if(do_hex) {
			char outFilename[256];

			unsigned size = dataSect.size;
			if(do_dhexSize!= 0) {
				if(do_dhexSize >= size) {
					size = do_dhexSize;
				}
				else {
					printf("*** WARNING *** - dhexSize: 0x%0x > 0x%0x\n",
						dataSect.size, do_dhexSize);
				}
			}
			char* dataBuf = (char*)calloc(size, 1);
			memcpy(dataBuf, dataSect.buf, dataSect.size);

			// Output in 32 wide
			sprintf(outFilename, "%s_d.hex", baseFilename);
			// saveIntel(outFilename, dataSect.buf, dataSect.size);
			saveIntel(outFilename, dataBuf, size);

			// byte 0, output 4 chunks
			// unsigned size = dataSect.size;
			unsigned char* buf0 = (unsigned char*)malloc(size/4);
			unsigned char* buf1 = (unsigned char*)malloc(size/4);
			unsigned char* buf2 = (unsigned char*)malloc(size/4);
			unsigned char* buf3 = (unsigned char*)malloc(size/4);
			for(unsigned i=0,k=0; i<size; i+=4,++k) {
				// buf[k] = dataSect.buf[i];
				buf0[k] = dataBuf[i];
				buf1[k] = dataBuf[i+1];
				buf2[k] = dataBuf[i+2];
				buf3[k] = dataBuf[i+3];
			}
			sprintf(outFilename, "%s_d0.hex", baseFilename);
			saveIntel(outFilename, buf0, size/4);
			sprintf(outFilename, "%s_d1.hex", baseFilename);
			saveIntel(outFilename, buf1, size/4);
			sprintf(outFilename, "%s_d2.hex", baseFilename);
			saveIntel(outFilename, buf2, size/4);
			sprintf(outFilename, "%s_d3.hex", baseFilename);
			saveIntel(outFilename, buf3, size/4);

		}
		if(do_bin) {
			char outFilename[256];
			sprintf(outFilename, "%s_d.bin", baseFilename);
			saveBinary(outFilename, dataSect.buf, dataSect.size);
		}
		if(do_mif) {
			char outFilename[256];
			sprintf(outFilename, "%s_d.mif", baseFilename);
			saveMif(outFilename, dataSect.buf, dataSect.size);
		}
		if(do_mif_i) {
			char outFilename[256];
			sprintf(outFilename, "%s_d_i.mif", baseFilename);
			saveMif_i(outFilename, dataSect.buf, dataSect.size);


			// byte 0, output 4 chunks
			unsigned size = dataSect.size;
			unsigned char* buf = (unsigned char*)malloc(size/4);
			for(unsigned i=0,k=0; i<size; i+=4,++k) {
				buf[k] = dataSect.buf[i];
			}
			sprintf(outFilename, "%s_d0_i.mif", baseFilename);
			saveMif8_i(outFilename, buf, size/4);

			// byte 1
			size = dataSect.size;
			buf = (unsigned char*)malloc(size/4);
			for(unsigned i=1,k=0; i<size; i+=4,++k) {
				buf[k] = dataSect.buf[i];
			}
			sprintf(outFilename, "%s_d1_i.mif", baseFilename);
			saveMif8_i(outFilename, buf, size/4);

			// byte 2
			size = dataSect.size;
			buf = (unsigned char*)malloc(size/4);
			for(unsigned i=2,k=0; i<size; i+=4,++k) {
				buf[k] = dataSect.buf[i];
			}
			sprintf(outFilename, "%s_d2_i.mif", baseFilename);
			saveMif8_i(outFilename, buf, size/4);

			// byte 3
			size = dataSect.size;
			buf = (unsigned char*)malloc(size/4);
			for(unsigned i=3,k=0; i<size; i+=4,++k) {
				buf[k] = dataSect.buf[i];
			}
			sprintf(outFilename, "%s_d3_i.mif", baseFilename);
			saveMif8_i(outFilename, buf, size/4);

		}
	}
#endif

#if 0
	// output initialized data
	if(do_dinit) {
		initData(code, 0x000, 0x200);
	}
#endif




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



