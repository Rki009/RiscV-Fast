
:again
cls

set "TEXT_BASE=0x00000000"
set "TEXT_SIZE=0x00100000"
set "DATA_BASE=0x40000000"
set "DATA_SIZE=0x00100000"
set "OPT=-march=rv32im -mabi=ilp32 -Wl,-Ttext=%TEXT_BASE% -Wl,-Tdata=%DATA_BASE%"

wsl /opt/riscv_11.1/bin/riscv64-unknown-elf-gcc %OPT% hello.c -o Hello.elf
@if errorlevel 1 goto oops

:: Explicit Memory:
:: wsl ../../RiscVFast --newlib -T%TEXT_BASE%:%TEXT_SIZE% -D%DATA_BASE%:%DATA_SIZE% Hello.elf

:: Used default: -T0x00000000:0x00100000 -D0x40000000:0x00100000
wsl ../../RiscVFast --newlib Hello.elf

@pause
@goto again

:oops
@echo ****  OOPS  ****
@pause
@goto again

