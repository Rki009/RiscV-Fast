#pragma once



#define RISCV_RV32
//- #define RV64

#ifdef RISCV_RV32
#define RV_XLEN	32
#define CRS_MISA_MXL	0b01		// XLEN = 32 bits
#endif
#ifdef RISCV_RV64
#define RV_XLEN	64
#define CRS_MISA_MXL	0b10		// XLEN = 64 bits
#endif

// MISA - Machine Instruction Architecture
//	ISA - Extensions
#define MISA_A		0x0000001	// Atomic extension
#define MISA_B		0x0000002	// Tentatively reserved for Bit-Manipulation extension
#define MISA_C		0x0000004	// Compressed extension
#define MISA_D		0x0000008	// Double-precision floating-point extension
#define MISA_E		0x0000010	// RV32E base ISA
#define MISA_F		0x0000020	// Single-precision floating-point extension
#define MISA_G		0x0000040	// Additional standard extensions present
#define MISA_H		0x0000080	// Hypervisor extension
#define MISA_I		0x0000100	// RV32I/64I/128I base ISA
#define MISA_J		0x0000200	// Tentatively reserved for Dynamically Translated Languages extension
#define MISA_K		0x0000400	// Reserved
#define MISA_L		0x0001800	// Tentatively reserved for Decimal Floating-Point extension
#define MISA_M		0x0001000	// Integer Multiply/Divide extension
#define MISA_N		0x0002000	// User-level interrupts supported
#define MISA_O		0x0004000	// Reserved
#define MISA_P		0x0008000	// Tentatively reserved for Packed-SIMD extension
#define MISA_Q		0x0010000	// Quad-precision floating-point extension
#define MISA_R		0x0020000	// Reserved
#define MISA_S		0x0040000	// Supervisor mode implemented
#define MISA_T		0x0080000	// Tentatively reserved for Transactional Memory extension
#define MISA_U		0x0100000	// User mode implemented
#define MISA_V		0x0200000	// Tentatively reserved for Vector extension
#define MISA_W		0x0400000	// Reserved
#define MISA_X		0x0800000	// Non-standard extensions present
#define MISA_Y		0x1000000	// Reserved
#define MISA_Z		0x2000000	// Reserved


//- `define CRS_MISA_EXT	26'h0000042	// Extensions = ???
#ifdef OP_COMPRESSED
#ifdef OP_MUL
#define CRS_MISA_EXT	(MISA_C | MISA_I | MISA_M)	// RV32ICM
#else
#define CRS_MISA_EXT	(MISA_C | MISA_I)			// RV32IC
#endif
#else
#ifdef OP_MUL
#define CRS_MISA_EXT	(MISA_I | MISA_M)			// RV32IM
#else
#define CRS_MISA_EXT	(MISA_I)					// RV32I
#endif
#endif

// CRS Type + rs1/imm5
#define CSR_TYPE_W	1		// VLIW_CSRRW
#define CSR_TYPE_S	2		// VLIW_CSRRS
#define CSR_TYPE_C	3		// VLIW_CSRRC

// Production Version
#define OPT_MVENDOR_ID	0x7fff2021
#define OPT_MARCH_ID	0x00007201
#define OPT_MIMP_ID		0x00000012
#define OPT_MHART_ID	0x00009001
#define OPT_MISA		((CRS_MISA_MXL<<30) | CRS_MISA_EXT)



// Machine Trap Setup
#define RISCV_CSR_MSTATUS	0x300		// mstatus		Machine Status
#define RISCV_CSR_MISA		0x301		// misa			Machine ISA and Extensions
#define RISCV_CSR_MIE		0x304		// mie			Machine Interrupt Enable
#define RISCV_CSR_MTVEC		0x305		// mtvec		Machine Trap Handler

// Machine Trap Handling
#define RISCV_CSR_MSCRATCH	0x340		// mscratch		Machine Scratch Register
#define RISCV_CSR_MEPC		0x341		// mepc			Machine Exception Program Counter
#define RISCV_CSR_MCAUSE	0x342		// mcause		Machine Exception Cause
#define RISCV_CSR_MTVAL		0x343		// mtval		Machine Trap Value
#define RISCV_CSR_MIP		0x344		// mip			Machine Interrupt Pending

// Machine Counter/Timers
#define RISCV_CSR_MCYCLE	0xb00		// mcycle		Clock Cycles Executed Counter
#define RISCV_CSR_MINSTRET	0xb02		// minstret		Number of Instructions Retired Counter
#define RISCV_CSR_MCYCLEH	0xb80		// mcycleh		Upper 32 bits of mcycle, RV32I only
#define RISCV_CSR_MINSTRETH	0xb82		// minstreth	Upper 32 bits of minstret, RV32I only
#define RISCV_CSR_CYCLE		0xc00		// cycle		Cycle counter for RDCYCLE instruction
#define RISCV_CSR_TIME		0xc01		// time			Timer for RDTIME instruction
#define RISCV_CSR_CYCLEH	0xc80		// cycleh		Upper 32 bits of cycle, RV32I only
#define RISCV_CSR_TIMEH		0xc81		// timeh		Upper 32 bits of time, RV32I only

// Machine Info
#define RISCV_CSR_MVENDORID	0xf11		// mvendorid	Machine Vendor ID
#define RISCV_CSR_MARCHID	0xf12		// marchid		Machine Architecture ID
#define RISCV_CSR_MIMPID	0xf13		// mimpid		Machine Implementation ID
#define RISCV_CSR_MHARTID	0xf14		// mhartid		Machine Hardware Thread ID

// Custom
#define RISCV_CSR_RAND		0x015		// rand32		32 bit random value

#define RISCV_MCAUSE_EBREAK	0x00000000	// EBREAK - Break Exception


//	Interrupts
#define RISCV_INTR_RSV_0	0		// Reserved
#define RISCV_INTR_SSI		1		// Supervisor software interrupt
#define RISCV_INTR_RSV_2	2		// Reserved
#define RISCV_INTR_MSI		3		// Machine software interrupt
#define RISCV_INTR_RSV_4	4		// Reserved
#define RISCV_INTR_STI		5		// Supervisor timer interrupt
#define RISCV_INTR_RSV_6	6		// Reserved
#define RISCV_INTR_MTI		7		// Machine timer interrupt
#define RISCV_INTR_RSV_8	8		// Reserved
#define RISCV_INTR_SEI		9		// Supervisor external interrupt
#define RISCV_INTR_RSV_10	10		// Reserved
#define RISCV_INTR_MEI		11		// Machine external interrupt
#define RISCV_INTR_RSV_12	12		// Reserved
#define RISCV_INTR_RSV_13	13		// Reserved
#define RISCV_INTR_RSV_14	14		// Reserved
#define RISCV_INTR_RSV_15	15		// Reserved

// Exceptions
#define RISCV_EXCP_IADDR 	0		// Instruction address misaligned
#define RISCV_EXCP_IACCESS 	1		// Instruction access fault
#define RISCV_EXCP_ILLEGAL 	2		// Illegal instruction
#define RISCV_EXCP_BREAK 	3		// Breakpoint
#define RISCV_EXCP_LMISS 	4		// Load address misaligned
#define RISCV_EXCP_LACCESS 	5		// Load access fault
#define RISCV_EXCP_SMISS	6		// Store/AMO address misaligned
#define RISCV_EXCP_SACCESS 	7		// Store/AMO access fault
#define RISCV_EXCP_ECALL_U 	8		// Environment call from U-mode
#define RISCV_EXCP_ECALL_S 	9		// Environment call from S-mode
#define RISCV_EXCP_RSV_10	10		// Reserved
#define RISCV_EXCP_ECLAA_M	11		// Environment call from M-mode
#define RISCV_EXCP_IPAGE	12		// Instruction page fault
#define RISCV_EXCP_LPAGE	13		// Load page fault
#define RISCV_EXCP_RSV_14	14		// Reserved
#define RISCV_EXCP_SPAGE	15		// Store/AMO page fault


#define RISCV_MSTATUS_UIE 	0x00000001
#define RISCV_MSTATUS_SIE 	0x00000002
#define RISCV_MSTATUS_HIE 	0x00000004
#define RISCV_MSTATUS_MIE 	0x00000008
#define RISCV_MSTATUS_UPIE	0x00000010
#define RISCV_MSTATUS_SPIE	0x00000020
#define RISCV_MSTATUS_HPIE	0x00000040
#define RISCV_MSTATUS_MPIE	0x00000080
#define RISCV_MSTATUS_SPP 	0x00000100
#define RISCV_MSTATUS_HPP 	0x00000600
#define RISCV_MSTATUS_MPP 	0x00001800
#define RISCV_MSTATUS_FS  	0x00006000
#define RISCV_MSTATUS_XS  	0x00018000
#define RISCV_MSTATUS_MPRV	0x00020000
#define RISCV_MSTATUS_SUM 	0x00040000
#define RISCV_MSTATUS_MXR 	0x00080000
#define RISCV_MSTATUS_TVM 	0x00100000
#define RISCV_MSTATUS_TW  	0x00200000
#define RISCV_MSTATUS_TSR 	0x00400000
#define RISCV_MSTATUS32_SD	0x80000000


#define RISCV_MIP_SSIP		0x00000002	// Supervisor software interrupt pending
#define RISCV_MIP_MSIP		0x00000008	// Machine software interrupt pending
#define RISCV_MIP_STIP		0x00000020	// Supervisor timer interrupt pending
#define RISCV_MIP_MTIP		0x00000080	// Machine timer interrupt pending
#define RISCV_MIP_SEIP		0x00000200	// Supervisor external interrupt pending
#define RISCV_MIP_MEIP		0x00000800	// Machine external interrupt pending

#define RISCV_MIE_SSIE		0x00000002	// Supervisor software interrupt enable
#define RISCV_MIE_MSIE		0x00000008	// Machine software interrupt enable
#define RISCV_MIE_STIE		0x00000020	// Supervisor timer interrupt enable
#define RISCV_MIE_MTIE		0x00000080	// Machine timer interrupt enable
#define RISCV_MIE_SEIE		0x00000200	// Supervisor external interrupt enable
#define RISCV_MIE_MEIE		0x00000800	// Machine external interrupt enable





