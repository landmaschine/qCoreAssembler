// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Data-driven instruction definitions for the qCore assembler
//              To add a new instruction, simply add an entry to INSTRUCTIONS table
// ============================================================================

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <sstream>

// Instruction format types - determines how operands are parsed and encoded
enum class InstrFormat {
    REG_REG,          // op rX, rY           (register to register)
    REG_IMM,          // op rX, #imm         (immediate only)
    REG_IMM_OR_REG,   // op rX, rY | #imm    (either register or immediate)
    BRANCH,           // b<cond> label       (branch with condition code)
    REG_ONLY,         // push/pop rX         (single register operand)
    REG_MEM,          // ld/st rX, [rY]      (memory access with register indirect)
    SHIFT,            // lsl/lsr/asr/ror rX, rY | #imm (shift operations)
    LABEL_LOAD,       // mv rX, =label       (load label address - generates MVT+ADD)
};

// Instruction definition entry
struct InstructionDef {
    std::string mnemonic;   // Assembly mnemonic (e.g., "add", "mv", "beq")
    InstrFormat format;     // How to parse and encode this instruction
    uint16_t opcodeReg;     // Opcode for register variant
    uint16_t opcodeImm;     // Opcode for immediate variant (0 if N/A)
    int immBits;            // Number of bits for immediate field
    uint8_t extraData;      // Format-specific data (e.g., branch condition, shift type)
    int baseSize;           // Base instruction size in words (1 or 2)
    bool canExpand;         // True if instruction can expand (e.g., =label generates 2 words)
    std::string description;// Human-readable description for documentation
    
    InstructionDef(const std::string& m, InstrFormat f, uint16_t opR, uint16_t opI, 
                   int bits, uint8_t extra = 0, int size = 1, bool expand = false,
                   const std::string& desc = "")
        : mnemonic(m), format(f), opcodeReg(opR), opcodeImm(opI), immBits(bits), 
          extraData(extra), baseSize(size), canExpand(expand), description(desc) {}
};

// ============================================================================
// INSTRUCTION TABLE - Add new instructions 
// ============================================================================
// Format: {mnemonic, format, opcodeReg, opcodeImm, immBits, extraData, size, canExpand, description}
//
// To add a new instruction:
// 1. Determine the format (REG_REG, REG_IMM, etc.)
// 2. Add a single line to this table
// ============================================================================

inline const std::vector<InstructionDef>& getInstructionTable() {
    static const std::vector<InstructionDef> INSTRUCTIONS = {
        // Data Movement Instructions
        {"mv",    InstrFormat::REG_IMM_OR_REG, 0x0000, 0x1000, 9, 0, 1, true,  "Move register or immediate to register"},
        {"mvt",   InstrFormat::REG_IMM,        0x3000, 0x3000, 8, 0, 1, false, "Move to top byte of register"},
        
        // ALU Instructions
        {"add",   InstrFormat::REG_IMM_OR_REG, 0x4000, 0x5000, 9, 0, 1, true,  "Add register or immediate"},
        {"sub",   InstrFormat::REG_IMM_OR_REG, 0x6000, 0x7000, 9, 0, 1, true,  "Subtract register or immediate"},
        {"and",   InstrFormat::REG_IMM_OR_REG, 0xC000, 0xD000, 9, 0, 1, true,  "Bitwise AND register or immediate"},
        {"xor",   InstrFormat::REG_REG,        0xE110, 0xE110, 0, 0, 1, false, "Bitwise XOR registers"},
        
        // Compare Instruction
        {"cmp",   InstrFormat::REG_IMM_OR_REG, 0xE000, 0xF000, 9, 0, 1, false, "Compare register with register or immediate"},
        
        // Memory Instructions
        {"ld",    InstrFormat::REG_MEM,        0x8000, 0x8000, 0, 0, 1, false, "Load from memory"},
        {"st",    InstrFormat::REG_MEM,        0xA000, 0xA000, 0, 0, 1, false, "Store to memory"},
        {"push",  InstrFormat::REG_ONLY,       0xB000, 0xB000, 0, 0x05, 1, false, "Push register to stack"},
        {"pop",   InstrFormat::REG_ONLY,       0x9000, 0x9000, 0, 0x05, 1, false, "Pop from stack to register"},
        
        // Shift Instructions (extraData = shift type: 0=LSL, 1=LSR, 2=ASR, 3=ROR)
        {"lsl",   InstrFormat::SHIFT,          0xE000, 0xE000, 4, 0, 1, false, "Logical shift left"},
        {"lsr",   InstrFormat::SHIFT,          0xE000, 0xE000, 4, 1, 1, false, "Logical shift right"},
        {"asr",   InstrFormat::SHIFT,          0xE000, 0xE000, 4, 2, 1, false, "Arithmetic shift right"},
        {"ror",   InstrFormat::SHIFT,          0xE000, 0xE000, 4, 3, 1, false, "Rotate right"},
        
        // Branch Instructions (extraData = condition code)
        {"b",     InstrFormat::BRANCH,         0x2000, 0x2000, 9, 0, 1, false, "Unconditional branch"},
        {"beq",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 1, 1, false, "Branch if equal (Z=1)"},
        {"bne",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 2, 1, false, "Branch if not equal (Z=0)"},
        {"bcc",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 3, 1, false, "Branch if carry clear (C=0)"},
        {"bcs",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 4, 1, false, "Branch if carry set (C=1)"},
        {"bpl",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 5, 1, false, "Branch if positive (N=0)"},
        {"bmi",   InstrFormat::BRANCH,         0x2000, 0x2000, 9, 6, 1, false, "Branch if negative (N=1)"},
        {"bl",    InstrFormat::BRANCH,         0x2000, 0x2000, 9, 7, 1, false, "Branch and link (call)"},
    };
    return INSTRUCTIONS;
}

inline const std::unordered_map<std::string, const InstructionDef*>& getInstructionMap() {
    static std::unordered_map<std::string, const InstructionDef*> map;
    static bool initialized = false;
    
    if (!initialized) {
        for (const auto& instr : getInstructionTable()) {
            map[instr.mnemonic] = &instr;
        }
        initialized = true;
    }
    return map;
}

inline bool isValidInstruction(const std::string& mnemonic) {
    return getInstructionMap().count(mnemonic) > 0;
}

inline const InstructionDef* getInstructionDef(const std::string& mnemonic) {
    auto& map = getInstructionMap();
    auto it = map.find(mnemonic);
    return (it != map.end()) ? it->second : nullptr;
}

// Register definitions
struct RegisterDef {
    std::string name;
    uint8_t number;
    std::string description;
    
    RegisterDef(const std::string& n, uint8_t num, const std::string& desc = "")
        : name(n), number(num), description(desc) {}
};

inline const std::vector<RegisterDef>& getRegisterTable() {
    static const std::vector<RegisterDef> REGISTERS = {
        {"r0", 0, "General purpose register 0"},
        {"r1", 1, "General purpose register 1"},
        {"r2", 2, "General purpose register 2"},
        {"r3", 3, "General purpose register 3"},
        {"r4", 4, "General purpose register 4"},
        {"r5", 5, "General purpose register 5 / Stack Pointer"},
        {"r6", 6, "General purpose register 6 / Link Register"},
        {"r7", 7, "General purpose register 7 / Program Counter"},
        {"sp", 5, "Stack Pointer (alias for r5)"},
        {"lr", 6, "Link Register (alias for r6)"},
        {"pc", 7, "Program Counter (alias for r7)"},
    };
    return REGISTERS;
}

inline const std::unordered_map<std::string, uint8_t>& getRegisterMap() {
    static std::unordered_map<std::string, uint8_t> regMap;
    static bool initialized = false;
    
    if (!initialized) {
        for (const auto& reg : getRegisterTable()) {
            regMap[reg.name] = reg.number;
        }
        initialized = true;
    }
    return regMap;
}

inline bool isValidRegister(const std::string& reg) {
    return getRegisterMap().count(reg) > 0;
}

// Directive definitions
struct DirectiveDef {
    std::string name;
    std::string description;
    
    DirectiveDef(const std::string& n, const std::string& desc = "")
        : name(n), description(desc) {}
};

inline const std::vector<DirectiveDef>& getDirectiveTable() {
    static const std::vector<DirectiveDef> DIRECTIVES = {
        {".word",   "Emit a 16-bit word value"},
        {".define", "Define a symbolic constant"},
    };
    return DIRECTIVES;
}

inline const std::unordered_map<std::string, const DirectiveDef*>& getDirectiveMap() {
    static std::unordered_map<std::string, const DirectiveDef*> map;
    static bool initialized = false;
    
    if (!initialized) {
        for (const auto& dir : getDirectiveTable()) {
            map[dir.name] = &dir;
        }
        initialized = true;
    }
    return map;
}

inline bool isValidDirective(const std::string& dir) {
    return getDirectiveMap().count(dir) > 0;
}

// Format string helpers for error messages and documentation
inline std::string getFormatString(InstrFormat format) {
    switch (format) {
        case InstrFormat::REG_REG:        return "rX, rY";
        case InstrFormat::REG_IMM:        return "rX, #imm";
        case InstrFormat::REG_IMM_OR_REG: return "rX, rY | rX, #imm | rX, =label";
        case InstrFormat::BRANCH:         return "label";
        case InstrFormat::REG_ONLY:       return "rX";
        case InstrFormat::REG_MEM:        return "rX, [rY]";
        case InstrFormat::SHIFT:          return "rX, rY | rX, #imm";
        case InstrFormat::LABEL_LOAD:     return "rX, =label";
        default:                          return "unknown";
    }
}

inline std::string getFormatHint(const InstructionDef* def) {
    if (!def) return "";
    return "Expected format: " + def->mnemonic + " " + getFormatString(def->format);
}

// Documentation generator
inline std::string generateInstructionSetDoc() {
    std::ostringstream doc;
    doc << "# qCore Instruction Set Reference\n\n";
    doc << "Auto-generated from instruction definitions.\n\n";
    
    // Instructions by category
    doc << "## Data Movement\n\n";
    doc << "| Mnemonic | Format | Description |\n";
    doc << "|----------|--------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.mnemonic == "mv" || instr.mnemonic == "mvt") {
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << instr.description << " |\n";
        }
    }
    
    doc << "\n## Arithmetic/Logic\n\n";
    doc << "| Mnemonic | Format | Description |\n";
    doc << "|----------|--------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.mnemonic == "add" || instr.mnemonic == "sub" || 
            instr.mnemonic == "and" || instr.mnemonic == "xor" ||
            instr.mnemonic == "cmp") {
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << instr.description << " |\n";
        }
    }
    
    doc << "\n## Memory\n\n";
    doc << "| Mnemonic | Format | Description |\n";
    doc << "|----------|--------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.mnemonic == "ld" || instr.mnemonic == "st" || 
            instr.mnemonic == "push" || instr.mnemonic == "pop") {
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << instr.description << " |\n";
        }
    }
    
    doc << "\n## Shift/Rotate\n\n";
    doc << "| Mnemonic | Format | Description |\n";
    doc << "|----------|--------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.format == InstrFormat::SHIFT) {
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << instr.description << " |\n";
        }
    }
    
    doc << "\n## Branch\n\n";
    doc << "| Mnemonic | Format | Condition | Description |\n";
    doc << "|----------|--------|-----------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.format == InstrFormat::BRANCH) {
            std::string cond;
            switch (instr.extraData) {
                case 0: cond = "Always"; break;
                case 1: cond = "Z=1"; break;
                case 2: cond = "Z=0"; break;
                case 3: cond = "C=0"; break;
                case 4: cond = "C=1"; break;
                case 5: cond = "N=0"; break;
                case 6: cond = "N=1"; break;
                case 7: cond = "Link"; break;
            }
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << cond << " | " << instr.description << " |\n";
        }
    }
    
    doc << "\n## Registers\n\n";
    doc << "| Name | Number | Description |\n";
    doc << "|------|--------|-------------|\n";
    for (const auto& reg : getRegisterTable()) {
        doc << "| `" << reg.name << "` | " << (int)reg.number 
            << " | " << reg.description << " |\n";
    }
    
    doc << "\n## Directives\n\n";
    doc << "| Directive | Description |\n";
    doc << "|-----------|-------------|\n";
    for (const auto& dir : getDirectiveTable()) {
        doc << "| `" << dir.name << "` | " << dir.description << " |\n";
    }
    
    return doc.str();
}
