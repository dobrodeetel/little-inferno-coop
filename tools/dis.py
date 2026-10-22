"""Disassemble the game exe at Ghidra VAs. Usage: python -I dis.py <va> [count]"""
import sys
import capstone
import pefile

pe = pefile.PE(sys.argv[3] if len(sys.argv) > 3 else "../li-re/LittleInferno_code.exe", fast_load=True)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
va = int(sys.argv[1], 16)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 16
for i, ins in enumerate(md.disasm(pe.get_data(va - 0x400000, n * 16), va)):
    if i >= n:
        break
    print(hex(ins.address), ins.bytes.hex(), ins.mnemonic, ins.op_str)
