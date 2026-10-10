#include "common.h"
#include "cpu6502.h"

#define OPC( n, am, o, cyc ) m_opcodes[o].name = #n; m_opcodes[o].addressMode = AM_##am;\
     m_opcodes[o].opc = o; m_opcodes[o].cycles = cyc;


Cpu6502::Opcode gOpcodeUnknown = { "???", Cpu6502::AM_Imp, 0x00, 2 };

int gAddressingModeSize[] =
{
    1,  //    AM_Imp,              // operand
    2,  //    AM_Imm,            // operand #value
    2,  //    AM_Relative,             // operand value
    2,  //    AM_Zero,             // operand value
    2,  //    AM_ZeroX,            // operand value,x
    2,  //    AM_ZeroY,            // operand value,y
    2,  //    AM_IndX,            // operand (value, x)
    2,  //    AM_IndY,            // operand (value), y
    3,  //    AM_Abs,             // operand value
    3,  //    AM_AbsX,            // operand value,x
    3,  //    AM_AbsY,            // operand value,y
    3   //    AM_Ind              // operand (value)
};

const char* gAddressingModeName[] =
{
    "Imp", "Imm", "Rel", "Zero", "Zero X", "Zero Y", "Ind X", "Ind Y", "Abs", "Abs X", "Abs Y", "Ind"
};

Cpu6502::Cpu6502()
{
    for (int i = 0; i < 256; i++)
    {
        m_opcodes[i] = gOpcodeUnknown;
    }

    // declare all the opcodes into the m_opcodes array and auto generate the decode function hooks
    OPC(ADC, Imm, 0x69, 2);
    OPC(ADC, Zero,  0x65, 3);
    OPC(ADC, ZeroX, 0x75, 4);
    OPC(ADC, Abs,  0x6D, 4);
    OPC(ADC, AbsX, 0x7D, 4);
    OPC(ADC, AbsY, 0x79, 4);
    OPC(ADC, IndX, 0x61, 6);
    OPC(ADC, IndY, 0x71, 5);

    OPC(AND, Imm, 0x29, 2);
    OPC(AND, Zero,  0x25, 3);
    OPC(AND, ZeroX, 0x35, 4);
    OPC(AND, Abs,  0x2D, 4);
    OPC(AND, AbsX, 0x3D, 4);
    OPC(AND, AbsY, 0x39, 4);
    OPC(AND, IndX, 0x21, 6);
    OPC(AND, IndY, 0x31, 5);

    OPC(ASL, Imp,   0x0A, 2);
    OPC(ASL, Zero,  0x06, 5);
    OPC(ASL, ZeroX, 0x16, 6);
    OPC(ASL, Abs,  0x0E, 6);
    OPC(ASL, AbsX, 0x1E, 7);

    OPC(BCC, Rel,  0x90, 2);
    OPC(BCS, Rel,  0xB0, 2);
    OPC(BEQ, Rel,  0xF0, 2);

    OPC(BIT, Zero,  0x24, 3);
    OPC(BIT, Abs,  0x2C, 4);

    OPC(BMI, Rel,  0x30, 2);
    OPC(BNE, Rel,  0xD0, 2);
    OPC(BPL, Rel,  0x10, 2);

    OPC(BRK, Imp,   0x00, 7);

    OPC(BVC, Rel,  0x50, 2);
    OPC(BVS, Rel,  0x70, 2);

    OPC(CLC, Imp,   0x18, 2);
    OPC(CLD, Imp,   0xD8, 2);
    OPC(CLI, Imp,   0x58, 2);
    OPC(CLV, Imp,   0xB8, 2);

    OPC(CMP, Imm, 0xC9, 2);
    OPC(CMP, Zero,  0xC5, 3);
    OPC(CMP, ZeroX, 0xD5, 4);
    OPC(CMP, Abs,  0xCD, 4);
    OPC(CMP, AbsX, 0xDD, 4);
    OPC(CMP, AbsY, 0xD9, 4);
    OPC(CMP, IndX, 0xC1, 6);
    OPC(CMP, IndY, 0xD1, 5);

    OPC(CPX, Imm, 0xE0, 2);
    OPC(CPX, Zero,  0xE4, 3);
    OPC(CPX, Abs,  0xEC, 4);

    OPC(CPY, Imm, 0xC0, 2);
    OPC(CPY, Zero,  0xC4, 3);
    OPC(CPY, Abs,  0xCC, 4);

    OPC(DEC, Zero,  0xC6, 5);
    OPC(DEC, ZeroX, 0xD6, 6);
    OPC(DEC, Abs,  0xCE, 6);
    OPC(DEC, AbsX, 0xDE, 7);

    OPC(DEX, Imp,   0xCA, 2);
    OPC(DEY, Imp,   0x88, 2);

    OPC(EOR, Imm, 0x49, 2);
    OPC(EOR, Zero,  0x45, 3);
    OPC(EOR, ZeroX, 0x55, 4);
    OPC(EOR, Abs,  0x4D, 4);
    OPC(EOR, AbsX, 0x5D, 4);
    OPC(EOR, AbsY, 0x59, 4);
    OPC(EOR, IndX, 0x41, 6);
    OPC(EOR, IndY, 0x51, 5);

    OPC(INC, Zero,  0xE6, 5);
    OPC(INC, ZeroX, 0xF6, 6);
    OPC(INC, Abs,  0xEE, 6);
    OPC(INC, AbsX, 0xFE, 7);

    OPC(INX, Imp,   0xE8, 2);
    OPC(INY, Imp,   0xC8, 2);

    OPC(JMP, Abs,  0x4C, 3);
    OPC(JMP, Ind,  0x6C, 5);

    OPC(JSR, Abs,  0x20, 6);

    OPC(LDA, Imm, 0xA9, 2);
    OPC(LDA, Zero,  0xA5, 3);
    OPC(LDA, ZeroX, 0xB5, 4);
    OPC(LDA, Abs,  0xAD, 4);
    OPC(LDA, AbsX, 0xBD, 4);
    OPC(LDA, AbsY, 0xB9, 4);
    OPC(LDA, IndX, 0xA1, 6);
    OPC(LDA, IndY, 0xB1, 5);

    OPC(LDX, Imm, 0xA2, 2);
    OPC(LDX, Zero,  0xA6, 3);
    OPC(LDX, ZeroY, 0xB6, 4);
    OPC(LDX, Abs,  0xAE, 4);
    OPC(LDX, AbsY, 0xBE, 4);

    OPC(LDY, Imm, 0xA0, 2);
    OPC(LDY, Zero,  0xA4, 3);
    OPC(LDY, ZeroX, 0xB4, 4);
    OPC(LDY, Abs,  0xAC, 4);
    OPC(LDY, AbsX, 0xBC, 4);

    OPC(LSR, Imp,   0x4A, 2);
    OPC(LSR, Zero,  0x46, 5);
    OPC(LSR, ZeroX, 0x56, 6);
    OPC(LSR, Abs,  0x4E, 6);
    OPC(LSR, AbsX, 0x5E, 7);

    OPC(NOP, Imp,   0xEA, 2);

    OPC(ORA, Imm, 0x09, 2);
    OPC(ORA, Zero,  0x05, 3);
    OPC(ORA, ZeroX, 0x15, 4);
    OPC(ORA, Abs,  0x0D, 4);
    OPC(ORA, AbsX, 0x1D, 4);
    OPC(ORA, AbsY, 0x19, 4);
    OPC(ORA, IndX, 0x01, 6);
    OPC(ORA, IndY, 0x11, 5);

    OPC(PHA, Imp,   0x48, 3);
    OPC(PHP, Imp,   0x08, 3);
    OPC(PLA, Imp,   0x68, 4);
    OPC(PLP, Imp,   0x28, 4);

    OPC(ROL, Imp, 0x2A, 2);
    OPC(ROL, Zero, 0x26, 5);
    OPC(ROL, ZeroX, 0x36, 6);
    OPC(ROL, Abs, 0x2E, 6);
    OPC(ROL, AbsX, 0x3E, 7);

    OPC(ROR, Imp, 0x6A, 2);
    OPC(ROR, Zero,  0x66, 5);
    OPC(ROR, ZeroX, 0x76, 6);
    OPC(ROR, Abs,  0x4E, 6);
    OPC(ROR, AbsX, 0x7E, 7);

    OPC(RTI, Imp,   0x40, 6);
    OPC(RTS, Imp,   0x60, 6);

    OPC(SBC, Imm, 0xE9, 2);
    OPC(SBC, Zero,  0xE5, 3);
    OPC(SBC, ZeroX, 0xF5, 4);
    OPC(SBC, Abs,  0xED, 4);
    OPC(SBC, AbsX, 0xFD, 4);
    OPC(SBC, AbsY, 0xF9, 4);
    OPC(SBC, IndX, 0xE1, 6);
    OPC(SBC, IndY, 0xF1, 5);

    OPC(SEC, Imp,   0x38, 2);
    OPC(SED, Imp,   0xF8, 2);
    OPC(SEI, Imp,   0x78, 2);

    OPC(STA, Zero,  0x85, 3);
    OPC(STA, ZeroX, 0x95, 4);
    OPC(STA, Abs,  0x8D, 4);
    OPC(STA, AbsX, 0x9D, 5);
    OPC(STA, AbsY, 0x99, 5);
    OPC(STA, IndX, 0x81, 6);
    OPC(STA, IndY, 0x91, 6);

    OPC(STX, Zero,  0x86, 3);
    OPC(STX, ZeroY, 0x96, 4);
    OPC(STX, Abs,  0x8E, 4);

    OPC(STY, Zero,  0x84, 3);
    OPC(STY, ZeroX, 0x94, 4);
    OPC(STY, Abs,  0x8C, 4);

    OPC(TAX, Imp,   0xAA, 3);
    OPC(TAY, Imp,   0xA8, 3);
    OPC(TSX, Imp,   0xBA, 3);
    OPC(TXA, Imp,   0x8A, 3);
    OPC(TXS, Imp,   0x9A, 3);
    OPC(TYA, Imp,   0x98, 3);
}

int Cpu6502::GetAddressingModeSize(AddressingMode am)
{
    return gAddressingModeSize[am];
}
const char *Cpu6502::GetAddressingModeName(AddressingMode am)
{
    return gAddressingModeName[am];
}

void Cpu6502::Disassemble(u16 addr, u8 opcode, u8 operand1, u8 operand2, std::string& opcodeBuffer, std::string& operandBuffer, int& bytes, int& cycles)
{
    auto op = m_opcodes[opcode];
    opcodeBuffer = op.name;
    bytes = gAddressingModeSize[op.addressMode];
    cycles = op.cycles;
    u16 absolute = ((int)operand1 << 16) | (int)operand2;
    switch (op.addressMode)
    {
        case AM_Imp:             // operand
            operandBuffer = "";
            break;
        case AM_Imm:             // operand #value
            operandBuffer = std::format("#${:02x}", operand1);
            break;
        case AM_Rel:             // operand value
            operandBuffer = std::format("${:04x}", addr + (int)operand1 - 128);
            break;
        case AM_Zero:            // operand value
            operandBuffer = std::format("${:02x}", operand1);
            break;
        case AM_ZeroX:           // operand value,x
            operandBuffer = std::format("${:02x},x", operand1);
            break;
        case AM_ZeroY:           // operand value,y
            operandBuffer = std::format("${:02x},y", operand1);
            break;
        case AM_IndX:            // operand (value, x)
            operandBuffer = std::format("(${:04x},x)", absolute);
            break;
        case AM_IndY:            // operand (value), y
            operandBuffer = std::format("(${:04x}),y", absolute);
            break;
        case AM_Abs:             // operand value
            operandBuffer = std::format("(${:04x}),y", absolute);
            break;
        case AM_AbsX:            // operand value,x
            operandBuffer = std::format("${:04x},x", absolute);
            break;
        case AM_AbsY:            // operand value,y
            operandBuffer = std::format("${:04x},y", absolute);
            break;
        case AM_Ind:              // operand (value)
            operandBuffer = std::format("(${:04x})", absolute);
            break;
    }
}


