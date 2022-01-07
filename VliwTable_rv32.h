#define VLIW_NONE             0x00
#define VLIW_LUI              0x01
#define VLIW_AUIPC            0x02
#define VLIW_J                0x03
#define VLIW_JAL              0x04
#define VLIW_JR               0x05
#define VLIW_JALR             0x06
#define VLIW_BEQ              0x07
#define VLIW_BNE              0x08
#define VLIW_BLT              0x09
#define VLIW_BGE              0x0a
#define VLIW_BLTU             0x0b
#define VLIW_BGEU             0x0c
#define VLIW_LB               0x0d
#define VLIW_LH               0x0e
#define VLIW_LW               0x0f
#define VLIW_LBU              0x10
#define VLIW_LHU              0x11
#define VLIW_SB               0x12
#define VLIW_SH               0x13
#define VLIW_SW               0x14
#define VLIW_NOP              0x15
#define VLIW_ADDI             0x16
#define VLIW_SLTI             0x17
#define VLIW_SLTIU            0x18
#define VLIW_XORI             0x19
#define VLIW_ORI              0x1a
#define VLIW_ANDI             0x1b
#define VLIW_SLLI             0x1c
#define VLIW_SRLI             0x1d
#define VLIW_SRAI             0x1e
#define VLIW_ADD              0x1f
#define VLIW_SUB              0x20
#define VLIW_SLL              0x21
#define VLIW_SLT              0x22
#define VLIW_SLTU             0x23
#define VLIW_XOR              0x24
#define VLIW_SRL              0x25
#define VLIW_SRA              0x26
#define VLIW_OR               0x27
#define VLIW_AND              0x28
#define VLIW_FENCE            0x29
#define VLIW_ECALL            0x2a
#define VLIW_EBREAK           0x2b
#define VLIW_SRET             0x2c
#define VLIW_MRET             0x2d
#define VLIW_WFI              0x2e
#define VLIW_FENCE_I          0x2f
#define VLIW_CSRRW            0x30
#define VLIW_CSRRS            0x31
#define VLIW_CSRRC            0x32
#define VLIW_CSRRWI           0x33
#define VLIW_CSRRSI           0x34
#define VLIW_CSRRCI           0x35
#define VLIW_MUL              0x36
#define VLIW_MULH             0x37
#define VLIW_MULHSU           0x38
#define VLIW_MULHU            0x39
#define VLIW_DIV              0x3a
#define VLIW_DIVU             0x3b
#define VLIW_REM              0x3c
#define VLIW_REMU             0x3d
#define VLIW_MULW             0x3e
#define VLIW_DIVW             0x3f
#define VLIW_DIVUW            0x40
#define VLIW_REMW             0x41
#define VLIW_REMUW            0x42
#define VLIW_LR_W             0x43
#define VLIW_SC_W             0x44
#define VLIW_AMOSWAP_W        0x45
#define VLIW_AMOADD_W         0x46
#define VLIW_AMOXOR_W         0x47
#define VLIW_AMOAND_W         0x48
#define VLIW_AMOOR_W          0x49
#define VLIW_AMOMIN_W         0x4a
#define VLIW_AMOMAX_W         0x4b
#define VLIW_AMOMINU_W        0x4c
#define VLIW_AMOMAXU_W        0x4d
#define VLIW_LR_D             0x4e
#define VLIW_SC_D             0x4f
#define VLIW_AMOSWAP_D        0x50
#define VLIW_AMOADD_D         0x51
#define VLIW_AMOXOR_D         0x52
#define VLIW_AMOAND_D         0x53
#define VLIW_AMOOR_D          0x54
#define VLIW_AMOMIN_D         0x55
#define VLIW_AMOMAX_D         0x56
#define VLIW_AMOMINU_D        0x57
#define VLIW_AMOMAXU_D        0x58
#define VLIW_FLW              0x59
#define VLIW_FSW              0x5a
#define VLIW_FMADD_S          0x5b
#define VLIW_FMSUB_S          0x5c
#define VLIW_FNMSUB_S         0x5d
#define VLIW_FNMADD_S         0x5e
#define VLIW_FADD_S           0x5f
#define VLIW_FSUB_S           0x60
#define VLIW_FMUL_S           0x61
#define VLIW_FDIV_S           0x62
#define VLIW_FSQRT_S          0x63
#define VLIW_FSGNJ_S          0x64
#define VLIW_FSGNJN_S         0x65
#define VLIW_FSGNJX_S         0x66
#define VLIW_FMIN_S           0x67
#define VLIW_FMAX_S           0x68
#define VLIW_FCVT_W_S         0x69
#define VLIW_FCVT_WU_S        0x6a
#define VLIW_FMV_X_W          0x6b
#define VLIW_FEQ_S            0x6c
#define VLIW_FLT_S            0x6d
#define VLIW_FLE_S            0x6e
#define VLIW_FCLASS_S         0x6f
#define VLIW_FCVT_S_W         0x70
#define VLIW_FCVT_S_WU        0x71
#define VLIW_FMV_W_X          0x72
#define VLIW_FCVT_L_S         0x73
#define VLIW_FCVT_LU_S        0x74
#define VLIW_FCVT_S_L         0x75
#define VLIW_FCVT_S_LU        0x76
#define VLIW_FLD              0x77
#define VLIW_FSD              0x78
#define VLIW_FMADD_D          0x79
#define VLIW_FMSUB_D          0x7a
#define VLIW_FNMSUB_D         0x7b
#define VLIW_FNMADD_D         0x7c
#define VLIW_FADD_D           0x7d
#define VLIW_FSUB_D           0x7e
#define VLIW_FMUL_D           0x7f
#define VLIW_FDIV_D           0x80
#define VLIW_FSQRT_D          0x81
#define VLIW_FSGNJ_D          0x82
#define VLIW_FSGNJN_D         0x83
#define VLIW_FSGNJX_D         0x84
#define VLIW_FMIN_D           0x85
#define VLIW_FMAX_D           0x86
#define VLIW_FCVT_S_D         0x87
#define VLIW_FCVT_D_S         0x88
#define VLIW_FEQ_D            0x89
#define VLIW_FLT_D            0x8a
#define VLIW_FLE_D            0x8b
#define VLIW_FCLASS_D         0x8c
#define VLIW_FCVT_W_D         0x8d
#define VLIW_FCVT_WU_D        0x8e
#define VLIW_FCVT_D_W         0x8f
#define VLIW_FCVT_D_WU        0x90
#define VLIW_FCVT_L_D         0x91
#define VLIW_FCVT_LU_D        0x92
#define VLIW_FMV_X_D          0x93
#define VLIW_FCVT_D_L         0x94
#define VLIW_FCVT_D_LU        0x95
#define VLIW_FMV_D_X          0x96
#define VLIW_FLQ              0x97
#define VLIW_FSQ              0x98
#define VLIW_FMADD_Q          0x99
#define VLIW_FMSUB_Q          0x9a
#define VLIW_FNMSUB_Q         0x9b
#define VLIW_FNMADD_Q         0x9c
#define VLIW_FADD_Q           0x9d
#define VLIW_FSUB_Q           0x9e
#define VLIW_FMUL_Q           0x9f
#define VLIW_FDIV_Q           0xa0
#define VLIW_FSQRT_Q          0xa1
#define VLIW_FSGNJ_Q          0xa2
#define VLIW_FSGNJN_Q         0xa3
#define VLIW_FSGNJX_Q         0xa4
#define VLIW_FMIN_Q           0xa5
#define VLIW_FMAX_Q           0xa6
#define VLIW_FCVT_S_Q         0xa7
#define VLIW_FCVT_Q_S         0xa8
#define VLIW_FCVT_D_Q         0xa9
#define VLIW_FCVT_Q_D         0xaa
#define VLIW_FEQ_Q            0xab
#define VLIW_FLT_Q            0xac
#define VLIW_FLE_Q            0xad
#define VLIW_FCLASS_Q         0xae
#define VLIW_FCVT_W_Q         0xaf
#define VLIW_FCVT_WU_Q        0xb0
#define VLIW_FCVT_Q_W         0xb1
#define VLIW_FCVT_Q_WU        0xb2
#define VLIW_FCVT_L_Q         0xb3
#define VLIW_FCVT_LU_Q        0xb4
#define VLIW_FCVT_Q_L         0xb5
#define VLIW_FCVT_Q_LU        0xb6
#define VLIW_ILLEGAL          0xb7
#define VLIW_C_ADDI4SPN       0xb8
#define VLIW_C_FLD            0xb9
#define VLIW_C_LQ             0xba
#define VLIW_C_LW             0xbb
#define VLIW_C_FLW            0xbc
#define VLIW_RESERVED1        0xbd
#define VLIW_C_FSD            0xbe
#define VLIW_C_SQ             0xbf
#define VLIW_C_SW             0xc0
#define VLIW_C_FSW            0xc1
#define VLIW_C_NOP            0xc2
#define VLIW_C_ADDI           0xc3
#define VLIW_C_JAL            0xc4
#define VLIW_C_ADDIW          0xc5
#define VLIW_C_LI             0xc6
#define VLIW_C_ADDI16SP       0xc7
#define VLIW_C_LUI            0xc8
#define VLIW_C_SRLI           0xc9
#define VLIW_C_SRAI           0xca
#define VLIW_C_ANDI           0xcb
#define VLIW_C_SUB            0xcc
#define VLIW_C_XOR            0xcd
#define VLIW_C_OR             0xce
#define VLIW_C_AND            0xcf
#define VLIW_C_SUBW           0xd0
#define VLIW_C_ADDW           0xd1
#define VLIW_RESERVED2        0xd2
#define VLIW_RESERVED3        0xd3
#define VLIW_C_J              0xd4
#define VLIW_C_BEQZ           0xd5
#define VLIW_C_BNEZ           0xd6
#define VLIW_C_SLLI           0xd7
#define VLIW_C_SLLI64         0xd8
#define VLIW_C_FLDSP          0xd9
#define VLIW_C_LQSP           0xda
#define VLIW_C_LWSP           0xdb
#define VLIW_C_FLWSP          0xdc
#define VLIW_C_LDSP           0xdd
#define VLIW_C_JR             0xde
#define VLIW_C_MV             0xdf
#define VLIW_C_EBREAK         0xe0
#define VLIW_C_JALR           0xe1
#define VLIW_C_ADD            0xe2
#define VLIW_C_FSDSP          0xe3
#define VLIW_C_SQSP           0xe4
#define VLIW_C_SWSP           0xe5
#define VLIW_C_FSWSP          0xe6
#define VLIW_C_SDSP           0xe7
#define VLIW_SIZE             0xe8