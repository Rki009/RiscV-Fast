# RiscVFAST - Very Fast RiscV ISA Simulator

## Features

- Very fast simulation of RiscV 32 architecture
	* RiscV ISA is converted in to VERY FAST VLIW (Very Long Instruction Word)
	* VLIW VERY FAST Execution Engine
	* Support for memory bounds/access, instruction logging, breakpoint debugging
	* Time interrupt, approximately 100Hz (programmable)
	* External interrupt from user defined source
	* mcycle CSR reports real cycles, 1 instruction per cycle, full 64 bits
	* time CSR reports real host time, in microseconds, full 64 bits
	* 32 bit random variable CSR support, CSR = 0x015
	* Simulation of 100MHz clocked Risc-V embedded systems in real-time!
	* 100+ million instructions per second on a modest x86 host 
	* 500+ DMIPS (900K Dhrystone/sec)
- RV32I ISA
	* Zicsr - Control Status Registers, basic support
	* C - Compression, slow
	* M - Future support
	* A - Future support
	* F - Future support
- RV64I ISA, Future support
- Bare Metal
	* Uart and Debug I/O ports
- Newlib
	* Ecall support for Newlib library interface to host file I/O system, time and other calls
- Memory
	* Direct loading of *.elf files
	* Support for full r/w memory or separate I/D memory

## Introduction
RiscVFAST has one main design goal, to execute Risc-V basic instructions as fast as possible to provide near real-time simulation of an embedded system or application. The Risc-V architecture is optimized for efficient fast execution when instantiated in hardware. It is a complex architecture to simulate in software. Extraction of register field, building immediate values from encoded instruction bits and generally parsing each instruction is complicated for a software simulator. The RiscVFAST simulator bypasses these complexities by decoding each instruction only once and creating a VLIW instruction for the FAST execution engine.
All subsequent accesses to the instruction result in the direct execution of the VLIW instruction without the need for decoding.

## VLIW Engine
The VLIW is a 64 bit instruction that contains an opcode, registers identifiers; rd, rs1, rs2 and a 32 bit immediate value.
The Risc-V ISA is mapped to the VLIW for direct execution. The VLIW engine does no decoding, it simply executes the VLIW opcode directly. For example the ADD opcode adds rs1 to rs2 and puts the result in rd. The 32 immediate value is ignored. No testing is done for and rd value of zero. So the value of x0 may be changed to non-zero. Of course this is invalid in the Risc-V architecture. So the VLIW simply rewrites zero into register x0 at the end of every instruction. This is faster than explicitly testing for x0 as a destination at execution time.

The goal is a FAST simulator. It certainly achieves its goal as a real-time simulator for the Risc-V architecture. There are some additional optimization that can speed up the VILW engine slightly, but the basic VILW engine is already very fast.

## Supported Instructions

    Load Upper:
    	lui rd,imm20
        auipc rd,imm20
    Jump:
        j joff
        jal rd,joff
        jr rs1
        jalr rd,rs1,simm12
    Branch:
    	beq rs1,rs2,boff
        bne rs1,rs2,boff
        blt rs1,rs2,boff
        bge rs1,rs2,boff
        bltu rs1,rs2,boff
        bgeu rs1,rs2,boff
    Load:
    	lb rd,rs,simm12
        lh rd,rs,simm12
        lw rd,rs,simm12
        lbu rd,rs,simm12
        lhu rd,rs,simm12
    Store:
    	sb rs2,rs1,s2imm12
        sh rs2,rs1,s2imm12
        sw rs2,rs1,s2imm12
    Alu: 
    	nop
        addi rd,rs,simm12
        slti rd,rs,simm12
        sltiu rd,rs,simm12
        xori rd,rs,simm12
        ori rd,rs,simm12
        andi rd,rs,simm12
        slli rd,rs1,shamt
        srli rd,rs1,shamt
        srai rd,rs1,shamt
        add rd,rs1,rs2
        sub rd,rs1,rs2
        sll rd,rs1,rs2
        slt rd,rs1,rs2
        sltu rd,rs1,rs2
        xor rd,rs1,rs2
        srl rd,rs1,rs2
        sra rd,rs1,rs2
        or rd,rs1,rs2
        and rd,rs1,rs2
    System:
    	ecall
        ebreak
        sret
        mret
        wfi
    CSR:
    	csrrw csr,rd,rs
        csrrs csr,rd,rs
        csrrc csr,rd,rs
        csrrwi csr,rd,uimm5
        csrrsi csr,rd,uimm5
        csrrci csr,rd,uimm5

## Build
A makefile is included. RiscVFAST has been built and tested using Ubuntu (linux) under Windows (wsl) and on a Raspberry Pi 4. 

## Future
Some future work ...
- Add support for M, A, F instructions
- Add support for real device interfaces; UART, SPI, I2C, Ethernet, ...

## Links
Some useful links ...

### Riscv.org:
https://riscv.org/technical/specifications/

### Unprivileged Spec:
https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf

### Privileged Spec:
https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf


## License

Copyright (c) 2022 Ron K. Irvine

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction for private use, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The Software may not be used in connection with any commercial purposes, except as
specifically approved by Ron K. Irvine or his representative. Unauthorized usage of
the Software or any part of the Software is prohibited. Appropriate legal action
will be taken by us for any illegal or unauthorized use of the Software.

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
