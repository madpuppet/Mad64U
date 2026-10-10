#pragma once

#include "Singleton.h"
#include <functional>

class Cpu6502 : public Singleton<Cpu6502>
{
public:
    Cpu6502();

    enum AddressingMode
    {
        AM_Imp,              // operand
        AM_Imm,             // operand #value
        AM_Rel,             // operand value
        AM_Zero,            // operand value
        AM_ZeroX,           // operand value,x
        AM_ZeroY,           // operand value,y
        AM_IndX,            // operand (value, x)
        AM_IndY,            // operand (value), y
        AM_Abs,             // operand value
        AM_AbsX,            // operand value,x
        AM_AbsY,            // operand value,y
        AM_Ind,             // operand (value)
        AM_Unknown          // .byte a,b,c 
    };

    enum CpuStatusRegisters
    {
        SR_Carry = 1,
        SR_Zero = 2,
        SR_Interrupt = 4,
        SR_Decimal = 8,
        SR_Break = 16,
        SR_Overflow = 64,
        SR_Negative = 128
    };

    struct Opcode
    {
        const char* name;
        AddressingMode addressMode;
        u8 opc;
        u8 cycles;
        u8 size;
    };

    Opcode* GetOpcode(u8 opcode) { return &m_opcodes[opcode]; }
    int GetAddressingModeSize(AddressingMode am);
    const char* GetAddressingModeName(AddressingMode am);
    void Disassemble(u16 addr, u8 opcode, u8 operand1, u8 operand2, std::string &opcodeBuffer, std::string &operandBuffer, int &bytes, int &cycles);

private:
    // array of 256 opcodes matching machine language value
    Opcode m_opcodes[256];
};

