//=====================================================================
//		Newlib System Call
//=====================================================================

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <sys/time.h>

#include <unistd.h>
#include <fcntl.h>

#include "RiscVFast.h"
#include "Cpu.h"
#include "Newlib-syscall.h"

#include "riscv_csr.h"


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
*/

// off_t lseek(int fd, off_t offset, int whence);
//	lseek() repositions the file offset of the open file description
//		associated with the file descriptor fd to the argument offset 
//		according to the directive whence.
int Cpu::sys_lseek(int fd, int offset, int whence) {
	printf("sys_lseek: fd = %d, offset = %08x, whence = %d\n", fd, offset, whence);
	if (fd <= 2) return -1;
	return ::lseek(fd, offset, whence);
}

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
	else if (fd == 1 || fd == 2) {
		return EOF;
	}
	else {
#if 0
		char* data = (char*)malloc(count);
		::read(fd, data, count);
		printf("Host Read: %s\n", data);
#endif
		return ::read(fd, buf, count);
		
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
	else if (fd <= 0) {
		return EOF;
	}
	else {
		return ::write(fd, buf, count);
	}

	return n;
}



// RiscV Newlib fcntl.h
#define RISCV_O_CREAT	0x0200  /* open with file create */
#define RISCV_O_TRUNC	0x0400  /* open with truncation */
#define RISCV_O_APPEND	0x0008  /* append (writes guaranteed at the end) */

#define RISCV_O_RDONLY	0		/* +1 == FREAD */
#define RISCV_O_WRONLY	1		/* +1 == FWRITE */
#define RISCV_O_RDWR	2		/* +1 == FREAD|FWRITE */

#define RISCV_O_DIRECTORY     0x200000
#define RISCV_O_TMPFILE       0x800000


int32_t Cpu::sys_open(uint32_t pathname, uint32_t flags, uint32_t mode) {
	// printf("sys_open: pathname=0x%08x, flags=0x%08x, mode=0x%08x\n", pathname, flags, mode);
	char fname[256];
	for (int i=0; i<(int)sizeof(fname)-1; ++i) {
		int c = memory->read8(pathname+i);
		fname[i] = c;
		fname[i+1] = '\0';
		if (c == '\0') break;
	}
	

	if (flags & RISCV_O_CREAT) printf("O_CREAT = %04x\n", RISCV_O_CREAT);
	if (flags & RISCV_O_APPEND) printf("O_APPEND = %04x\n", RISCV_O_APPEND);
	if (flags & RISCV_O_WRONLY) printf("O_WRONLY = %04x\n", RISCV_O_WRONLY);
	if (flags & RISCV_O_RDONLY) printf("O_RDONLY  = %04x\n", RISCV_O_RDONLY );
	if (flags & RISCV_O_RDWR) printf("O_RDWR  = %04x\n", RISCV_O_RDWR );
	if (flags & RISCV_O_TRUNC) printf("O_TRUNC = %04x\n", RISCV_O_TRUNC);
	if (flags & RISCV_O_TMPFILE) printf("O_TMPFILE  = %04x\n", RISCV_O_TMPFILE );
	// if (flags & RISCV_O_ASYNC) printf("O_ASYNC = %04x\n", RISCV_O_ASYNC);
	if (flags & RISCV_O_DIRECTORY) printf("O_DIRECTORY = %04x\n", RISCV_O_DIRECTORY);

	// flags need to be remapped from Newlib to Linux (Ubuntu)
	int new_flags = 0;
	if ((flags&0x3) == RISCV_O_RDONLY) new_flags |= O_RDONLY;
	if ((flags&0x3) == RISCV_O_WRONLY) new_flags |= O_WRONLY;
	if ((flags&0x3) == RISCV_O_RDWR) new_flags |= O_RDWR;

	if (flags & RISCV_O_CREAT) new_flags |= O_CREAT;
	if (flags & RISCV_O_APPEND) new_flags |= O_APPEND;
	if (flags & RISCV_O_TRUNC) new_flags |= O_TRUNC;

	printf("Open %s, Flags %04x, Mode = 0%03o\n", fname, flags, mode);
	printf("  new_flags = %04x\n", new_flags);
	// int fd = open(fname, flags, mode);
	// int fd = open(fname, O_RDONLY | O_CREAT);
	int fd = open(fname, new_flags, mode);

	printf("  fd = %d\n", fd);
	// return -1;	// Error
	return fd;	// Error
}

int32_t Cpu::sys_close(uint32_t fd) {
	(void)fd;
	printf("sys_close: fd=%d\n", fd);
	// return -1;	// Error
	
	// if stdin,stdout or stderr just keep open in the host
	if (fd <= 2) return 0;
	return close(fd);
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

	case SYS_lseek:
		reg[10] = sys_lseek(a0, a1, a2);
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

