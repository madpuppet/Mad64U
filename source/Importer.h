#pragma once

#include <string>

// Turns a C64 binary into KickAssembler source that assembles back to the same bytes and can be run with Ctrl-F5.
//
//  - PRG at $0801: disassembled in place; the BASIC SYS line gives the entry point.
//  - PRG elsewhere: wrapped in a BasicUpstart2 stub that copies it to its load address and jumps to it.
//  - SID (PSID/RSID): disassembled directly at its load address (* = LOAD) with a BasicUpstart2 stub that calls init
//    and then calls play from a raster (or CIA timer) interrupt. Tunes below $0810 are copied into place instead.
//
// Code is found by tracing from the entry points (and IRQ/NMI vectors the code installs); everything else is data.
// On success writes the .asm next to the input file and returns its path in asmPath.
bool ImportBinary(const std::string& path, std::string& asmPath, std::string& error);
