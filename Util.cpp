#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "RiscVFast.h"
#include "Cpu.h"


// format uint64_t into hex string
//	works for both long and long long
const char* hex64(uint64_t value) {
	static char buf[32];

	if(sizeof(long int) == 4) {
		sprintf(buf, "%016llx", (unsigned long long int)value);
	}
	else {
		sprintf(buf, "%016lx", (unsigned long int)value);
	}
	return buf;
}
// format int64_t into decimal string
//	works for both long and long long
const char* dec64(uint64_t value) {
	static char buf[32];

	if(sizeof(long int) == 4) {
		sprintf(buf, "%lld", (unsigned long long int)value);
	}
	else {
		sprintf(buf, "%ld", (unsigned long int)value);
	}
	return buf;
}



/*
 * Put out a fatal error.
 */
void fatal(const char* fmt, ...) {
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	printf("%s", buffer);
	cpu->dump32();
	va_end(args);
	exit(-1);
}

/*
 * Put out a warning
 */
void warn(const char* fmt, ...) {
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	printf("Warn: %s", buffer);
	va_end(args);
}

/*
 * Put out a error
 */
void error(const char* fmt, ...) {
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	printf("Error: %s", buffer);
	va_end(args);
}

void _trace(const char* file, const char* func, int line, const char* fmt, ...) {
	static FILE* tfp = NULL;
	char buffer[1024];

	if(tfp == NULL) {
		tfp = fopen("trace.txt", "w");
	}
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	// zap trailing '\n' if any
	int n = strlen(buffer);
	if(buffer[n-1] == '\n') {
		buffer[n-1] = '\0';
	}

	fprintf(tfp, "%s, %s.%d: %s\n", file, func, line, buffer);
	fflush(tfp);
	printf("Trace: %s, %s.%d: %s\n", file, func, line, buffer);
	va_end(args);
}


#ifdef __WIN32__
#include <windows.h>
void Sleep(int waitTime) {
	__int64 time1 = 0, time2 = 0, freq = 0;

	QueryPerformanceCounter((LARGE_INTEGER*) &time1);
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

	do {
		QueryPerformanceCounter((LARGE_INTEGER*) &time2);
	} while((time2-time1) < waitTime*1000);
}
#else
#include <unistd.h>
inline void Sleep(int ms) {
	usleep(ms*1000);
}
#endif

#if 0
void initMemory(char* filename) {
	FILE* fp = fopen(filename, "rb");
	if(fp == NULL) {
		printf("Unable to open %s\n", filename);
		return;
	}
	unsigned n = fread(insnMem, 1, sizeof(insnMem), fp);
	printf("Loaded %s, %04x bytes\n", filename, n);
	// fread(dataMem, 1, sizeof(dataMem), fp);

	if(fp) {
		fclose(fp);
	}
};

void initData(char* filename) {
	FILE* fp = fopen(filename, "rb");
	if(fp == NULL) {
		printf("Unable to open %s\n", filename);
		return;
	}
	// fread(insnMem, 1, sizeof(insnMem), fp);
	fread(dataMem, 1, sizeof(dataMem), fp);

	if(fp) {
		fclose(fp);
	}
};
#endif

