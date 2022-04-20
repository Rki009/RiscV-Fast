Simple "Hello World!" demo program.

Makefile:
	- PREFIX=/opt/riscv_11.1/bin/ is the prefix for my RiscV tools, change it to
		support your version of the RiscV compiler.
	- Fastest execution is when rv32 instruction compression is disabled and all
		instructions are 32 bits in length
	- Floating point is not supported ... yet
	- The default is separate Instruction and Data memory
		MEMORY = -T0x00000000:0x00100000 -D0x40000000:0x00100000
			Text (Instruction)	Base: 0x00000000, Length: 0x00100000
			Data				Base: 0x40000000, Length: 0x00100000
			
	- newlib (not linux) are supported with the --newlib switch when the program is run:
		../../RiscVFast --newlib $(MEMORY) Hello32.elf
		

NewLib:
	The newlib library can be used to provide semi-hosting. There is limited support
	for host file access including stdin/stdout/stderr.


Bare Metal Host/Devices:
	Bare Metal can be supported with the addition of customized code for support
	of bare metal devices such as uart/i2c/SPI. None is currently available in
	RiscVFast, but can easily be added using the I/O memory mapped address space.
	
	See "RiscVFast.h" for the following:
		// Fake devices and Host
		extern uint32_t	hostAddr;	// 0xffff0000
		extern uint32_t	hostSize;	// 64K
		uint32_t host(bool rw, uint32_t addr, uint32_t value);
		inline void host_write(uint32_t addr, uint32_t value) {
			host(true, addr, value);
			return;
		};
		inline uint32_t host_read(uint32_t addr) {
			return host(false, addr, 0);
		};
		
	To customize, add additional functionality to host_read() and host_write().

