#include "common.h"
#include "Importer.h"
#include "cpu6502.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <cstring>

namespace
{
    enum : u8
    {
        Mark_None,
        Mark_CodeStart,
        Mark_CodeCont,
    };

    enum : u8
    {
        Ref_Jump = 1,       // branch / jmp target
        Ref_Sub = 2,        // jsr target
        Ref_Data = 4,       // read / written as data
        Ref_Entry = 8,      // explicit entry point (named)
    };

    // vectors whose installation (lda #<x / sta vec / lda #>x / sta vec+1) marks x as code
    const u16 s_vectors[] = { 0x0314, 0x0316, 0x0318, 0xfffa, 0xfffc, 0xfffe };

    struct Disassembler
    {
        u32 m_load = 0;
        std::vector<u8> m_data;
        std::vector<u8> m_mark;
        std::vector<u8> m_refs;
        std::map<u32, std::string> m_names;     // explicit label names
        std::vector<std::string> m_aliases;     // extra names for already labelled addresses: "name = label"

        bool In(u32 addr) const { return addr >= m_load && addr < m_load + m_data.size(); }
        u8 At(u32 addr) const { return m_data[addr - m_load]; }

        const Cpu6502::Opcode* Op(u32 addr) const { return Cpu6502::Instance().GetOpcode(At(addr)); }

        static bool Is(const Cpu6502::Opcode* op, const char* name) { return _stricmp(op->name, name) == 0; }

        u32 Operand16(u32 addr) const { return At(addr + 1) | (At(addr + 2) << 8); }

        void Name(u32 addr, const std::string& name)
        {
            if (!In(addr))
                return;
            auto it = m_names.find(addr);
            if (it == m_names.end())
                m_names[addr] = name;
            else if (it->second != name)
                m_aliases.push_back(std::format(".label {} = {}", name, it->second));
            m_refs[addr - m_load] |= Ref_Entry;
        }

        void Trace(u32 entry)
        {
            std::vector<u32> work{ entry };
            u16 vecLo[sizeof(s_vectors) / sizeof(s_vectors[0])] = {};
            u16 vecHi[sizeof(s_vectors) / sizeof(s_vectors[0])] = {};
            u8 vecSet[sizeof(s_vectors) / sizeof(s_vectors[0])] = {};

            while (!work.empty())
            {
                u32 addr = work.back();
                work.pop_back();
                int regA = -1, regX = -1, regY = -1;

                while (In(addr))
                {
                    u32 idx = addr - m_load;
                    if (m_mark[idx] != Mark_None)
                        break;

                    auto op = Op(addr);
                    if (op->addressMode == Cpu6502::AM_Unknown)
                        break;
                    u32 size = op->size;
                    if (!In(addr + size - 1))
                        break;
                    bool clash = false;
                    for (u32 k = 1; k < size; k++)
                        clash |= m_mark[idx + k] != Mark_None;
                    if (clash)
                        break;

                    m_mark[idx] = Mark_CodeStart;
                    for (u32 k = 1; k < size; k++)
                        m_mark[idx + k] = Mark_CodeCont;

                    // control flow
                    if (op->addressMode == Cpu6502::AM_Rel)
                    {
                        work.push_back((addr + 2 + (i8)At(addr + 1)) & 0xffff);
                    }
                    else if (Is(op, "JSR"))
                    {
                        work.push_back(Operand16(addr));
                    }
                    else if (Is(op, "JMP"))
                    {
                        if (op->addressMode == Cpu6502::AM_Abs)
                            work.push_back(Operand16(addr));
                        break;
                    }
                    else if (Is(op, "RTS") || Is(op, "RTI") || Is(op, "BRK"))
                    {
                        break;
                    }

                    // interrupt vector installs
                    bool isStore = Is(op, "STA") || Is(op, "STX") || Is(op, "STY");
                    if (op->addressMode == Cpu6502::AM_Imm && Is(op, "LDA"))
                        regA = At(addr + 1);
                    else if (op->addressMode == Cpu6502::AM_Imm && Is(op, "LDX"))
                        regX = At(addr + 1);
                    else if (op->addressMode == Cpu6502::AM_Imm && Is(op, "LDY"))
                        regY = At(addr + 1);
                    else if (isStore && op->addressMode == Cpu6502::AM_Abs)
                    {
                        int value = Is(op, "STA") ? regA : Is(op, "STX") ? regX : regY;
                        u32 dest = Operand16(addr);
                        for (int v = 0; v < (int)(sizeof(s_vectors) / sizeof(s_vectors[0])); v++)
                        {
                            if (value < 0)
                                break;
                            if (dest == s_vectors[v])
                            {
                                vecLo[v] = (u16)value;
                                vecSet[v] |= 1;
                            }
                            else if (dest == s_vectors[v] + 1u)
                            {
                                vecHi[v] = (u16)value;
                                vecSet[v] |= 2;
                            }
                            if (vecSet[v] == 3)
                            {
                                u32 target = vecLo[v] | (vecHi[v] << 8);
                                vecSet[v] = 0;
                                if (In(target))
                                {
                                    Name(target, std::format("irq_{:04x}", target));
                                    work.push_back(target);
                                }
                            }
                        }
                    }
                    else if (!isStore && !Is(op, "SEI") && !Is(op, "CLI") && !Is(op, "NOP"))
                    {
                        regA = regX = regY = -1;
                    }

                    addr += size;
                }
            }
        }

        u32 InstructionStart(u32 addr) const
        {
            while (addr > m_load && m_mark[addr - m_load] == Mark_CodeCont)
                addr--;
            return addr;
        }

        // operand target of an instruction, or -1 if it has none worth labelling
        int Target(u32 addr) const
        {
            auto op = Op(addr);
            switch (op->addressMode)
            {
                case Cpu6502::AM_Rel:
                    return (addr + 2 + (i8)At(addr + 1)) & 0xffff;
                case Cpu6502::AM_Abs:
                case Cpu6502::AM_AbsX:
                case Cpu6502::AM_AbsY:
                case Cpu6502::AM_Ind:
                    return (int)Operand16(addr);
                default:
                    return -1;
            }
        }

        void CollectReferences()
        {
            for (u32 addr = m_load; In(addr); addr++)
            {
                if (m_mark[addr - m_load] != Mark_CodeStart)
                    continue;
                int target = Target(addr);
                if (target < 0x100 || !In((u32)target))
                    continue;

                auto op = Op(addr);
                u8 kind = Ref_Data;
                if (op->addressMode == Cpu6502::AM_Rel || (Is(op, "JMP") && op->addressMode == Cpu6502::AM_Abs))
                    kind = Ref_Jump;
                else if (Is(op, "JSR"))
                    kind = Ref_Sub;

                u32 t = (u32)target;
                if (m_mark[t - m_load] == Mark_CodeCont)
                    t = InstructionStart(t);
                m_refs[t - m_load] |= kind;
            }
        }

        bool HasLabel(u32 addr) const { return In(addr) && m_refs[addr - m_load] != 0; }

        std::string LabelName(u32 addr) const
        {
            auto it = m_names.find(addr);
            if (it != m_names.end())
                return it->second;
            u8 refs = m_refs[addr - m_load];
            if (refs & Ref_Sub)
                return std::format("S_{:04x}", addr);
            if (m_mark[addr - m_load] == Mark_CodeStart)
                return std::format("L_{:04x}", addr);
            return std::format("D_{:04x}", addr);
        }

        // how to write an address operand: a label (plus offset) inside the image, else the raw address
        std::string AddressText(u32 target, bool zeroPage) const
        {
            if (target >= 0x100 && In(target))
            {
                u32 start = m_mark[target - m_load] == Mark_CodeCont ? InstructionStart(target) : target;
                if (HasLabel(start))
                {
                    if (start == target)
                        return LabelName(start);
                    return std::format("{}+{}", LabelName(start), target - start);
                }
            }
            return zeroPage ? std::format("${:02x}", target) : std::format("${:04x}", target);
        }

        static std::string Lower(const char* s)
        {
            std::string r = s;
            for (auto& c : r)
                c = (char)tolower((unsigned char)c);
            return r;
        }

        std::string Instruction(u32 addr) const
        {
            auto op = Op(addr);
            std::string m = Lower(op->name);
            u8 b1 = op->size > 1 ? At(addr + 1) : 0;
            u32 abs = op->size > 2 ? Operand16(addr) : 0;

            // absolute addressing of a zero page address: the assembler would pick the shorter zero page form,
            // so keep the original bytes
            bool absMode = op->addressMode == Cpu6502::AM_Abs || op->addressMode == Cpu6502::AM_AbsX || op->addressMode == Cpu6502::AM_AbsY;
            if (absMode && abs < 0x100 && !Is(op, "JMP") && !Is(op, "JSR"))
            {
                const char* suffix = op->addressMode == Cpu6502::AM_AbsX ? ",x" : op->addressMode == Cpu6502::AM_AbsY ? ",y" : "";
                return std::format(".byte ${:02x},${:02x},${:02x}   // {} ${:04x}{}", At(addr), At(addr + 1), At(addr + 2), m, abs, suffix);
            }

            switch (op->addressMode)
            {
                case Cpu6502::AM_Imp: return m;
                case Cpu6502::AM_Imm: return std::format("{} #${:02x}", m, b1);
                case Cpu6502::AM_Rel: return std::format("{} {}", m, AddressText((addr + 2 + (i8)b1) & 0xffff, false));
                case Cpu6502::AM_Zero: return std::format("{} ${:02x}", m, b1);
                case Cpu6502::AM_ZeroX: return std::format("{} ${:02x},x", m, b1);
                case Cpu6502::AM_ZeroY: return std::format("{} ${:02x},y", m, b1);
                case Cpu6502::AM_IndX: return std::format("{} (${:02x},x)", m, b1);
                case Cpu6502::AM_IndY: return std::format("{} (${:02x}),y", m, b1);
                case Cpu6502::AM_Abs: return std::format("{} {}", m, AddressText(abs, false));
                case Cpu6502::AM_AbsX: return std::format("{} {},x", m, AddressText(abs, false));
                case Cpu6502::AM_AbsY: return std::format("{} {},y", m, AddressText(abs, false));
                case Cpu6502::AM_Ind: return std::format("{} ({})", m, AddressText(abs, false));
                default: return std::format(".byte ${:02x}", At(addr));
            }
        }

        static void Line(std::string& out, const std::string& text, const std::string& comment)
        {
            std::string line = "        " + text;
            if (!comment.empty())
            {
                if (line.size() < 48)
                    line.append(48 - line.size(), ' ');
                else
                    line += "  ";
                line += "// " + comment;
            }
            out += line + "\n";
        }

        // the disassembly of the whole image, indented for use inside a .pseudopc block or at the top level
        std::string Body() const
        {
            std::string out;
            u32 end = m_load + (u32)m_data.size();
            u32 addr = m_load;
            while (addr < end)
            {
                u32 idx = addr - m_load;
                if (HasLabel(addr))
                {
                    u8 refs = m_refs[idx];
                    if ((refs & (Ref_Sub | Ref_Entry)) && m_mark[idx] == Mark_CodeStart)
                        out += "\n//------------------------------------------------------------------------------\n";
                    out += LabelName(addr) + ":\n";
                }

                if (m_mark[idx] == Mark_CodeStart)
                {
                    auto op = Op(addr);
                    std::string bytes;
                    for (u32 k = 0; k < op->size; k++)
                        bytes += std::format("{}{:02x}", k ? " " : "", At(addr + k));
                    Line(out, Instruction(addr), std::format("${:04x}  {}", addr, bytes));
                    addr += op->size;
                    continue;
                }

                // data: up to 8 bytes, stopping at code or a label
                std::string text = ".byte ";
                u32 start = addr;
                int count = 0;
                while (addr < end && count < 8)
                {
                    if (addr != start && (HasLabel(addr) || m_mark[addr - m_load] != Mark_None))
                        break;
                    if (m_mark[addr - m_load] != Mark_None)
                        break;
                    text += std::format("{}${:02x}", count ? "," : "", At(addr));
                    addr++;
                    count++;
                }
                if (count == 0)
                {
                    // shouldn't happen (continuation byte without a start); emit it raw to stay byte exact
                    text += std::format("${:02x}", At(addr));
                    addr++;
                }
                Line(out, text, std::format("${:04x}", start));
            }

            for (auto& alias : m_aliases)
                out += alias + "\n";
            return out;
        }

        void Setup(u32 load, const u8* data, size_t size)
        {
            m_load = load;
            m_data.assign(data, data + size);
            m_mark.assign(size, Mark_None);
            m_refs.assign(size, 0);
        }
    };

    std::string Sanitize(const char* s, size_t maxLen)
    {
        std::string r;
        for (size_t i = 0; i < maxLen && s[i]; i++)
            r += (s[i] >= 0x20 && s[i] < 0x7f) ? s[i] : '?';
        return r;
    }

    // stub code that copies payload..payload_end to LOAD. 'backwards' when LOAD is above the payload.
    std::string CopyCode(bool backwards)
    {
        if (!backwards)
        {
            return
                "        // copy the payload down to LOAD (forwards, LOAD is below the payload)\n"
                "        lda #<payload\n"
                "        sta $fb\n"
                "        lda #>payload\n"
                "        sta $fc\n"
                "        lda #<LOAD\n"
                "        sta $fd\n"
                "        lda #>LOAD\n"
                "        sta $fe\n"
                "        ldy #0\n"
                "        ldx #>(payload_end - payload)\n"
                "        beq !partial+\n"
                "!page:  lda ($fb),y\n"
                "        sta ($fd),y\n"
                "        iny\n"
                "        bne !page-\n"
                "        inc $fc\n"
                "        inc $fe\n"
                "        dex\n"
                "        bne !page-\n"
                "!partial:\n"
                "        ldx #<(payload_end - payload)\n"
                "        beq !done+\n"
                "!byte:  lda ($fb),y\n"
                "        sta ($fd),y\n"
                "        iny\n"
                "        dex\n"
                "        bne !byte-\n"
                "!done:\n";
        }
        return
            "        // copy the payload up to LOAD (backwards from the end, LOAD is above the payload)\n"
            "        lda #<(payload + ((payload_end - payload) & $ff00))\n"
            "        sta $fb\n"
            "        lda #>(payload + ((payload_end - payload) & $ff00))\n"
            "        sta $fc\n"
            "        lda #<(LOAD + ((payload_end - payload) & $ff00))\n"
            "        sta $fd\n"
            "        lda #>(LOAD + ((payload_end - payload) & $ff00))\n"
            "        sta $fe\n"
            "        ldy #<(payload_end - payload)\n"
            "        beq !pages+\n"
            "!byte:  dey\n"
            "        lda ($fb),y\n"
            "        sta ($fd),y\n"
            "        cpy #0\n"
            "        bne !byte-\n"
            "!pages: ldx #>(payload_end - payload)\n"
            "        beq !done+\n"
            "!page:  dec $fc\n"
            "        dec $fe\n"
            "        ldy #$ff\n"
            "!byte:  lda ($fb),y\n"
            "        sta ($fd),y\n"
            "        dey\n"
            "        cpy #$ff\n"
            "        bne !byte-\n"
            "        dex\n"
            "        bne !page-\n"
            "!done:\n";
    }

    // Lays out a BasicUpstart2 program: the stub (from 'start') plus the payload assembled for LOAD.
    //  - LOAD at or above $0a00: stub first at $0810, payload after it, copied up to LOAD
    //  - LOAD in $0810-$09ff:   payload assembled directly at LOAD (nothing to copy), stub after it
    //  - LOAD below $0810:      payload at $0810, stub after it, copied down to LOAD
    std::string WrapWithStub(const std::string& constants, const std::string& stubAfterCopy, const std::string& body, u32 load)
    {
        std::string out = constants;
        out += "\n:BasicUpstart2(start)\n\n";

        std::string payload =
            "payload:\n"
            ".pseudopc LOAD {\n" + body + "}\n"
            "payload_end:\n";

        auto stub = [&](const std::string& copy)
            {
                return std::string(
                    "//==============================================================================\n"
                    "// Startup: copy the payload to where it runs from, then start it\n"
                    "//==============================================================================\n"
                    "start:\n"
                    "        sei\n"
                    "        cld\n"
                    "        lda #$34                                // all RAM while copying\n"
                    "        sta $01\n") + copy + stubAfterCopy;
            };

        if (load >= 0x0a00)
        {
            out += "* = $0810 \"Startup\"\n";
            out += stub(CopyCode(true));
            out += "\n//==============================================================================\n";
            out += "// Payload (assembled for LOAD, stored after the startup code)\n";
            out += "//==============================================================================\n";
            out += ".errorif payload > LOAD, \"startup code too large: payload must start below LOAD\"\n";
            out += payload;
        }
        else if (load >= 0x0810)
        {
            out += "//==============================================================================\n";
            out += "// Payload (assembled in place at LOAD)\n";
            out += "//==============================================================================\n";
            out += "* = LOAD \"Payload\"\n";
            out += payload;
            out += "\n";
            out += stub("        // the payload is already at LOAD\n");
        }
        else
        {
            out += "//==============================================================================\n";
            out += "// Payload (assembled for LOAD, stored at $0810)\n";
            out += "//==============================================================================\n";
            out += "* = $0810 \"Payload\"\n";
            out += payload;
            out += "\n";
            out += stub(CopyCode(false));
        }
        return out;
    }

    bool ReadFile(const std::string& path, std::vector<u8>& data)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return false;
        data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }

    // finds data/<name> under the working directory or next to the executable; empty path if missing
    std::filesystem::path FindDataFile(const char* name)
    {
        std::error_code ec;
        auto path = std::filesystem::current_path() / "data" / name;
        if (std::filesystem::exists(path, ec))
            return path;
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
        {
            path = std::filesystem::path(exePath).parent_path() / "data" / name;
            if (std::filesystem::exists(path, ec))
                return path;
        }
        return {};
    }

    // the LZSA2 decompressor source, data/decompress.asm
    bool ReadDecompressor(std::string& text, std::string& error)
    {
        auto path = FindDataFile("decompress.asm");
        std::vector<u8> data;
        if (path.empty() || !ReadFile(path.string(), data))
        {
            error = "can't find data/decompress.asm";
            return false;
        }

        // keep the line endings consistent with the rest of the generated file
        text.clear();
        for (u8 c : data)
            if (c != '\r')
                text += (char)c;
        return true;
    }

    // packs data into a raw LZSA2 block with data/lzsa.exe (lzsa -r -f2 in out)
    bool CompressLzsa2(const u8* data, size_t size, std::vector<u8>& packed, std::string& error)
    {
        auto lzsa = FindDataFile("lzsa.exe");
        if (lzsa.empty())
        {
            error = "can't find data/lzsa.exe";
            return false;
        }

        std::error_code ec;
        auto tempDir = std::filesystem::temp_directory_path(ec);
        auto id = std::format("mad64u_{}_{}", GetCurrentProcessId(), GetTickCount64());
        auto inPath = tempDir / (id + ".bin");
        auto outPath = tempDir / (id + ".lzsa");
        {
            std::ofstream f(inPath, std::ios::binary);
            if (!f)
            {
                error = "can't write a temporary file for lzsa";
                return false;
            }
            f.write((const char*)data, (std::streamsize)size);
        }

        std::wstring cmd = L"\"" + lzsa.wstring() + L"\" -r -f2 \"" + inPath.wstring() + L"\" \"" + outPath.wstring() + L"\"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        bool ok = false;
        DWORD exitCode = 1;
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            ok = exitCode == 0 && ReadFile(outPath.string(), packed) && !packed.empty();
        }
        std::filesystem::remove(inPath, ec);
        std::filesystem::remove(outPath, ec);
        if (!ok)
            error = std::format("lzsa.exe failed to compress the tune (exit code {})", exitCode);
        return ok;
    }

    // finds the SYS address in a BASIC program at $0801, or -1
    int FindSysAddress(const Disassembler& d)
    {
        u32 line = 0x0801;
        for (int lines = 0; lines < 16 && d.In(line + 4); lines++)
        {
            u32 next = d.At(line) | (d.At(line + 1) << 8);
            if (next == 0)
                break;
            for (u32 p = line + 4; d.In(p) && d.At(p) != 0; p++)
            {
                if (d.At(p) != 0x9e)        // SYS token
                    continue;
                p++;
                while (d.In(p) && (d.At(p) == ' ' || d.At(p) == '('))
                    p++;
                int value = 0, digits = 0;
                while (d.In(p) && d.At(p) >= '0' && d.At(p) <= '9' && digits < 6)
                {
                    value = value * 10 + (d.At(p) - '0');
                    p++;
                    digits++;
                }
                return digits ? value : -1;
            }
            if (next <= line)
                break;
            line = next;
        }
        return -1;
    }

    bool ImportPrg(const std::vector<u8>& file, const std::string& name, std::string& out, std::string& error)
    {
        if (file.size() < 3)
        {
            error = "file too small to be a PRG";
            return false;
        }
        u32 load = file[0] | (file[1] << 8);
        size_t size = (std::min)(file.size() - 2, (size_t)(0x10000 - load));

        Disassembler d;
        d.Setup(load, file.data() + 2, size);

        out = std::format("// Imported from {}\n// PRG: load address ${:04x}, {} bytes (${:04x}-${:04x})\n", name, load, size, load, load + size - 1);

        if (load == 0x0801)
        {
            int sys = FindSysAddress(d);
            if (sys >= 0 && d.In((u32)sys))
            {
                d.Name((u32)sys, "entry");
                d.Trace((u32)sys);
                out += std::format("// BASIC SYS {} (${:04x}) -> entry\n", sys, sys);
            }
            else
            {
                out += "// No SYS line found: the program is kept as data (BASIC program)\n";
            }
            d.CollectReferences();
            out += "\n* = $0801 \"Program\"\n";
            out += d.Body();
            return true;
        }

        // not at $0801: copy into place from a BASIC stub and jump to the load address
        d.Name(load, "entry");
        d.Trace(load);
        d.CollectReferences();

        std::string constants = std::format(
            "// Started from a BASIC stub which copies it to the load address and jumps to it.\n"
            "// ENTRY assumes execution starts at the load address; change it if the program starts elsewhere.\n"
            "\n"
            ".const LOAD = ${:04x}\n"
            ".const ENTRY = ${:04x}\n", load, load);

        std::string after =
            "        lda #$37                                // default memory map\n"
            "        sta $01\n"
            "        cli\n"
            "        jmp ENTRY\n";

        out += WrapWithStub(constants, after, d.Body(), load);
        return true;
    }

    u16 BE16(const std::vector<u8>& f, size_t o) { return (u16)((f[o] << 8) | f[o + 1]); }
    u32 BE32(const std::vector<u8>& f, size_t o) { return ((u32)f[o] << 24) | ((u32)f[o + 1] << 16) | ((u32)f[o + 2] << 8) | f[o + 3]; }

    bool ImportSid(const std::vector<u8>& file, const std::string& name, std::string& out, std::string& error)
    {
        if (file.size() < 0x76)
        {
            error = "file too small to be a SID";
            return false;
        }
        bool rsid = file[0] == 'R';
        u16 version = BE16(file, 4);
        u16 dataOffset = BE16(file, 6);
        u32 load = BE16(file, 8);
        u32 init = BE16(file, 10);
        u32 play = BE16(file, 12);
        u16 songs = BE16(file, 14);
        u16 startSong = BE16(file, 16);
        u32 speed = BE32(file, 18);

        if (dataOffset >= file.size())
        {
            error = "SID data offset is past the end of the file";
            return false;
        }
        size_t dataStart = dataOffset;
        if (load == 0)
        {
            if (dataStart + 2 > file.size())
            {
                error = "SID data too small";
                return false;
            }
            load = file[dataStart] | (file[dataStart + 1] << 8);
            dataStart += 2;
        }
        if (init == 0)
            init = load;
        size_t size = (std::min)(file.size() - dataStart, (size_t)(0x10000 - load));
        if (size == 0)
        {
            error = "SID has no data";
            return false;
        }
        if (startSong == 0)
            startSong = 1;
        int song = startSong - 1;
        bool ciaTiming = song < 32 && (speed & (1u << song)) != 0;

        std::string decompressor;
        if (!ReadDecompressor(decompressor, error))
            return false;
        std::vector<u8> packed;
        if (!CompressLzsa2(file.data() + dataStart, size, packed, error))
            return false;

        std::string title = Sanitize((const char*)&file[0x16], 32);
        std::string author = Sanitize((const char*)&file[0x36], 32);
        std::string released = Sanitize((const char*)&file[0x56], 32);

        out = std::format(
            "// Imported from {}\n"
            "// {} v{}: \"{}\" by {} ({})\n"
            "// Load ${:04x}-${:04x} ({} bytes), init ${:04x}, play ${:04x}, {} song(s), start song {}\n",
            name, rsid ? "RSID" : "PSID", version, title, author, released,
            load, load + size - 1, size, init, play, songs, startSong);

        std::string constants = std::format(
            "\n"
            ".const LOAD = ${:04x}\n"
            ".const INIT = ${:04x}\n"
            ".const PLAY = ${:04x}\n"
            ".const SONG = {}                               // song number - 1 passed to INIT in A\n"
            ".const SIZE = ${:04x}                          // unpacked size\n",
            load, init, play, song, size);

        std::string after;
        if (play == 0 || rsid)
        {
            // the tune installs its own interrupt in INIT
            bool underRom = (init >= 0xa000 && init < 0xc000) || init >= 0xe000;
            after = std::format(
                "        lda #${:02x}                                // {}\n"
                "        sta $01\n"
                "        lda #SONG\n"
                "        jsr INIT                                // the tune sets up its own interrupt\n"
                "        cli\n"
                "!loop:  jmp !loop-\n",
                underRom ? 0x35 : 0x37, underRom ? "RAM under the ROMs, I/O visible" : "default memory map");
        }
        else
        {
            after =
                "        lda #$35                                // RAM under the ROMs, I/O visible\n"
                "        sta $01\n"
                "        lda #<nmi\n"
                "        sta $fffa\n"
                "        lda #>nmi\n"
                "        sta $fffb\n"
                "        lda #<irq\n"
                "        sta $fffe\n"
                "        lda #>irq\n"
                "        sta $ffff\n"
                "        lda #$7f                                // stop the CIA interrupts\n"
                "        sta $dc0d\n"
                "        sta $dd0d\n"
                "        lda $dc0d\n"
                "        lda $dd0d\n"
                "        lda #0\n"
                "        sta $d01a\n"
                "        ldx #$18                                // silence the SID\n"
                "!clear: sta $d400,x\n"
                "        dex\n"
                "        bpl !clear-\n";
            if (ciaTiming)
            {
                after +=
                    "        lda #$25                                // CIA timer default (60Hz); INIT may change it\n"
                    "        sta $dc04\n"
                    "        lda #$40\n"
                    "        sta $dc05\n";
            }
            after +=
                "        lda #SONG\n"
                "        tax\n"
                "        tay\n"
                "        jsr INIT\n";
            if (ciaTiming)
            {
                after +=
                    "        lda #$81                                // this song is CIA timed: call PLAY from CIA 1 timer A\n"
                    "        sta $dc0d\n"
                    "        lda #$11\n"
                    "        sta $dc0e\n";
            }
            else
            {
                after +=
                    "        lda #$1b                                // call PLAY once a frame from a raster interrupt\n"
                    "        sta $d011\n"
                    "        lda #$30\n"
                    "        sta $d012\n"
                    "        lda #$01\n"
                    "        sta $d01a\n"
                    "        sta $d019\n";
            }
            after +=
                "        cli\n"
                "!loop:  jmp !loop-\n"
                "\n"
                "irq:    pha\n"
                "        txa\n"
                "        pha\n"
                "        tya\n"
                "        pha\n"
                "        lda #$ff                                // acknowledge the interrupt\n"
                "        sta $d019\n"
                "        lda $dc0d\n"
                "        jsr PLAY\n"
                "        pla\n"
                "        tay\n"
                "        pla\n"
                "        tax\n"
                "        pla\n"
                "nmi:    rti\n";
        }

        // where the startup code, decompressor and packed tune go: at $0810 if that is clear of the tune, else after it
        u32 blockSize = (u32)packed.size() + 0x200;     // packed data + generous room for the code
        u32 blockStart = 0x0810;
        if (blockStart < load + size && blockStart + blockSize > load)
        {
            blockStart = load + (u32)size;
            if (blockStart < 0x0810 || blockStart + blockSize > 0xd000)
            {
                error = std::format("no room for the player: the tune at ${:04x}-${:04x} leaves no free space above $0810", load, load + size - 1);
                return false;
            }
        }

        out += constants;
        out += "\n:BasicUpstart2(start)\n\n";
        out += std::format("* = ${:04x} \"Startup\"\n", blockStart);
        out +=
            "//==============================================================================\n"
            "// Startup: unpack the tune to LOAD, then play it\n"
            "//==============================================================================\n"
            "start:\n"
            "        sei\n"
            "        cld\n"
            "        lda #$34                                // all RAM while unpacking\n"
            "        sta $01\n"
            "        lda #<sid_packed\n"
            "        sta LZSA_SRC_LO\n"
            "        lda #>sid_packed\n"
            "        sta LZSA_SRC_HI\n"
            "        lda #<LOAD\n"
            "        sta LZSA_DST_LO\n"
            "        lda #>LOAD\n"
            "        sta LZSA_DST_HI\n"
            "        lda #$ff                                // start with a match offset of -1, so the end marker decodes\n"
            "        sta OFFSLO                              // correctly even if the data has no matches before it\n"
            "        sta OFFSHI\n"
            "        jsr DECOMPRESS\n";
        out += after;
        out +=
            "\n"
            "//==============================================================================\n"
            "// LZSA2 decompressor (data/decompress.asm)\n"
            "//==============================================================================\n";
        out += decompressor;
        if (!out.empty() && out.back() != '\n')
            out += "\n";
        out += std::format(
            "\n"
            "//==============================================================================\n"
            "// The tune: ${:04x} bytes packed with LZSA2 to {} bytes, unpacked to LOAD at startup\n"
            "//==============================================================================\n"
            "sid_packed:\n", size, packed.size());
        for (size_t i = 0; i < packed.size(); i += 16)
        {
            out += "        .byte ";
            for (size_t k = i; k < packed.size() && k < i + 16; k++)
                out += std::format("{}${:02x}", k == i ? "" : ",", packed[k]);
            out += "\n";
        }
        out +=
            "sid_block_end:\n"
            "\n"
            ".errorif start < LOAD + SIZE && sid_block_end > LOAD, \"the player overlaps the tune's memory\"\n";
        return true;
    }

    // a block of bytes as .byte lines
    std::string ByteLines(const u8* data, size_t size)
    {
        std::string out;
        for (size_t i = 0; i < size; i += 32)
        {
            out += "        .byte ";
            for (size_t k = i; k < size && k < i + 32; k++)
                out += std::format("{}${:02x}", k == i ? "" : ",", data[k]);
            out += "\n";
        }
        return out;
    }

    enum class ImageType
    {
        None,
        Koala,      // Koala Painter: multicolour bitmap, load $6000
        ArtStudio,  // Art Studio: hires bitmap, load $2000
    };

    ImageType DetectImage(const std::vector<u8>& file, const std::string& ext)
    {
        if (ext == ".prg")
            return ImageType::None;
        u32 load = file.size() >= 2 ? (file[0] | (file[1] << 8)) : 0;
        if (ext == ".kla" || ext == ".koa" || (load == 0x6000 && file.size() >= 10003 && file.size() <= 10010))
            return ImageType::Koala;
        if (ext == ".art" || (load == 0x2000 && file.size() >= 9002 && file.size() <= 9012))
            return ImageType::ArtStudio;
        return ImageType::None;
    }

    // Koala / Art Studio picture -> bitmap at $4000, screen map at $7000 (and for Koala the colour map at $7400,
    // copied to $d800 at startup), with a small viewer that shows it
    bool ImportImage(const std::vector<u8>& file, ImageType type, const std::string& name, std::string& out, std::string& error)
    {
        const u32 bitmapAddr = 0x4000;
        const u32 screenAddr = 0x7000;
        const u32 colourAddr = 0x7400;
        bool koala = type == ImageType::Koala;

        size_t needed = koala ? 2 + 10001 : 2 + 9000;
        if (file.size() < needed)
        {
            error = std::format("file too small for a {} picture ({} bytes, expected {})", koala ? "Koala" : "Art Studio", file.size(), needed);
            return false;
        }

        const u8* bitmap = &file[2];
        const u8* screen = &file[2 + 8000];
        const u8* colour = koala ? &file[2 + 9000] : nullptr;
        u8 background = koala ? (file[2 + 10000] & 15) : 0;
        u8 border = koala ? background : (file.size() > 2 + 9000 ? (file[2 + 9000] & 15) : 0);

        // VIC bank 1 ($4000-$7fff): screen at +$3000, bitmap at +$0000
        u8 d018 = (u8)((((screenAddr & 0x3fff) / 0x400) << 4) | (((bitmapAddr & 0x3fff) / 0x2000) << 3));

        out = std::format(
            "// Imported from {}\n"
            "// {}: bitmap -> ${:04x}, screen map -> ${:04x}{}\n"
            "\n"
            ".const BITMAP = ${:04x}\n"
            ".const SCREEN = ${:04x}\n",
            name, koala ? "Koala Painter multicolour bitmap" : "Art Studio hires bitmap", bitmapAddr, screenAddr,
            koala ? std::format(", colour map -> ${:04x} (copied to $d800)", colourAddr) : std::string(),
            bitmapAddr, screenAddr);
        if (koala)
            out += std::format(".const COLOUR = ${:04x}\n", colourAddr);

        out +=
            "\n"
            ":BasicUpstart2(start)\n"
            "\n"
            "* = $0810 \"Viewer\"\n"
            "start:\n"
            "        sei\n"
            "        lda #$0b                                // blank the screen while setting up\n"
            "        sta $d011\n";
        if (koala)
        {
            out +=
                "        ldx #0                                  // colour map -> colour RAM\n"
                "!copy:  lda COLOUR,x\n"
                "        sta $d800,x\n"
                "        lda COLOUR+$100,x\n"
                "        sta $d900,x\n"
                "        lda COLOUR+$200,x\n"
                "        sta $da00,x\n"
                "        lda COLOUR+$2e8,x\n"
                "        sta $dae8,x\n"
                "        inx\n"
                "        bne !copy-\n";
        }
        out += std::format(
            "        lda $dd02                               // VIC bank 1 ($4000-$7fff)\n"
            "        ora #$03\n"
            "        sta $dd02\n"
            "        lda $dd00\n"
            "        and #$fc\n"
            "        ora #$02\n"
            "        sta $dd00\n"
            "        lda #${:02x}                                // screen at SCREEN, bitmap at BITMAP\n"
            "        sta $d018\n"
            "        lda #${:02x}                                // {}\n"
            "        sta $d016\n"
            "        lda #${:02x}                                // background\n"
            "        sta $d021\n"
            "        lda #${:02x}                                // border\n"
            "        sta $d020\n"
            "        lda #$3b                                // bitmap mode, screen on\n"
            "        sta $d011\n"
            "        cli\n"
            "!loop:  jmp !loop-\n",
            d018, koala ? 0x18 : 0x08, koala ? "multicolour" : "hires", background, border);

        out += "\n* = BITMAP \"Bitmap\"\n" + ByteLines(bitmap, 8000);
        out += "\n* = SCREEN \"Screen map\"\n" + ByteLines(screen, 1000);
        if (koala)
            out += "\n* = COLOUR \"Colour map\"\n" + ByteLines(colour, 1000);
        return true;
    }
}

bool ImportBinary(const std::string& path, std::string& asmPath, std::string& error)
{
    std::vector<u8> file;
    if (!ReadFile(path, file))
    {
        error = std::format("can't read {}", path);
        return false;
    }

    std::filesystem::path p = path;
    std::string name = p.filename().string();
    std::string out;
    std::string ext = p.extension().string();
    for (auto& c : ext)
        c = (char)tolower((unsigned char)c);

    bool isSid = file.size() >= 4 && (memcmp(file.data(), "PSID", 4) == 0 || memcmp(file.data(), "RSID", 4) == 0);
    ImageType image = isSid ? ImageType::None : DetectImage(file, ext);
    bool ok = isSid ? ImportSid(file, name, out, error)
        : image != ImageType::None ? ImportImage(file, image, name, out, error)
        : ImportPrg(file, name, out, error);
    if (!ok)
        return false;

    // never overwrite an existing file
    std::filesystem::path target = p.parent_path() / (p.stem().string() + ".asm");
    for (int n = 1; std::filesystem::exists(target); n++)
        target = p.parent_path() / std::format("{}_import{}.asm", p.stem().string(), n == 1 ? std::string() : std::to_string(n));

    std::ofstream f(target, std::ios::binary);
    if (!f)
    {
        error = std::format("can't write {}", target.string());
        return false;
    }
    f << out;
    asmPath = target.string();
    return true;
}
