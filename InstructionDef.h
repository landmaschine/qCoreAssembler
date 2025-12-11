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
#include <iomanip>

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
    NO_OPERAND,       // halt                (no operands)
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

// INSTRUCTION TABLE - Add new instructions 
// Format: {mnemonic, format, opcodeReg, opcodeImm, immBits, extraData, size, canExpand, description}

inline const std::vector<InstructionDef>& getInstructionTable() {
    static const std::vector<InstructionDef> INSTRUCTIONS = {
        // Data Movement Instructions
        {"mv",    InstrFormat::REG_IMM_OR_REG, 0x0000, 0x1000, 9, 0, 1, true,  "Move register or immediate to register"},
        {"mvt",   InstrFormat::REG_IMM,        0x3000, 0x3000, 8, 0, 1, false, "Move to top byte of register"},
        
        // ALU Instructions
        {"add",   InstrFormat::REG_IMM_OR_REG, 0x4000, 0x5000, 9, 0, 1, true,  "Add register or immediate"},
        {"sub",   InstrFormat::REG_IMM_OR_REG, 0x6000, 0x7000, 9, 0, 1, true,  "Subtract register or immediate"},
        {"and",   InstrFormat::REG_IMM_OR_REG, 0xC000, 0xD000, 9, 0, 1, true,  "Bitwise AND register or immediate"},
        
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

        // Control Instructions
        {"halt",  InstrFormat::NO_OPERAND,     0xE1F0, 0xE1F0, 0, 0, 1, false, "Halt processor execution"},
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

// REGISTER DEFINITIONS

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

inline const char* getRegisterName(uint8_t regNum) {
    static const char* names[] = {"r0", "r1", "r2", "r3", "r4", "sp", "lr", "pc"};
    return (regNum < 8) ? names[regNum] : "??";
}

// DIRECTIVE DEFINITIONS

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
        {".org",    "Set the current assembly address (origin)"},
        {".space",  "Reserve N words of zero-initialized memory"},
        {".ascii",  "Emit a string as words (one char per word, no null terminator)"},
        {".asciiz", "Emit a null-terminated string (one char per word)"},
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

// DISASSEMBLY - Used by MIF writer

inline std::string disassembleInstruction(uint16_t instr, size_t address) {
    std::ostringstream oss;
    
    uint16_t opcode = (instr >> 13) & 0x7;
    uint16_t imm = (instr >> 12) & 0x1;
    uint16_t rX = (instr >> 9) & 0x7;
    uint16_t rY = instr & 0x7;
    uint16_t immediate9 = instr & 0x1FF;
    uint16_t immediate8 = instr & 0xFF;

    switch (opcode) {
        case 0: // MV register
            oss << "mv   " << getRegisterName(rX) << ", " << getRegisterName(rY);
            break;

        case 1: // MV immediate or Branch
            if (imm) {
                oss << "mvt  " << getRegisterName(rX) << ", #0x" << std::hex << immediate8;
            } else {
                static const char* conditions[] = {"b   ", "beq ", "bne ", "bcc ", "bcs ", "bpl ", "bmi ", "bl  "};
                int16_t offset = immediate9;
                if (offset & 0x100) offset |= 0xFE00;  // Sign extend
                int target = address + 1 + offset;
                oss << conditions[rX] << "0x" << std::hex << target;
            }
            break;

        case 2: // ADD
            if (imm) {
                oss << "add  " << getRegisterName(rX) << ", #0x" << std::hex << immediate9;
            } else {
                oss << "add  " << getRegisterName(rX) << ", " << getRegisterName(rY);
            }
            break;

        case 3: // SUB
            if (imm) {
                oss << "sub  " << getRegisterName(rX) << ", #0x" << std::hex << immediate9;
            } else {
                oss << "sub  " << getRegisterName(rX) << ", " << getRegisterName(rY);
            }
            break;

        case 4: // LD or POP
            if (imm) {
                oss << "pop  " << getRegisterName(rX);
            } else {
                oss << "ld   " << getRegisterName(rX) << ", [" << getRegisterName(rY) << "]";
            }
            break;

        case 5: // ST or PUSH
            if (imm) {
                oss << "push " << getRegisterName(rX);
            } else {
                oss << "st   " << getRegisterName(rX) << ", [" << getRegisterName(rY) << "]";
            }
            break;

        case 6: // AND
            if (imm) {
                oss << "and  " << getRegisterName(rX) << ", #0x" << std::hex << immediate9;
            } else {
                oss << "and  " << getRegisterName(rX) << ", " << getRegisterName(rY);
            }
            break;

        case 7: // CMP, Shifts, HALT
        {
            uint16_t shiftFlag = (instr >> 8) & 0x1;
            
            if (shiftFlag) {
                uint16_t immShift = (instr >> 7) & 0x1;
                uint16_t shiftType = (instr >> 5) & 0x3;
                uint16_t shiftAmount = instr & 0xF;
                
                // Check for HALT: 1110---11111----
                if (immShift && shiftType == 3 && (instr & 0x10)) {
                    oss << "halt";
                } else {
                    static const char* shiftTypes[] = {"lsl ", "lsr ", "asr ", "ror "};
                    oss << shiftTypes[shiftType] << getRegisterName(rX);
                    if (immShift) {
                        oss << ", #0x" << std::hex << shiftAmount;
                    } else {
                        oss << ", " << getRegisterName(rY);
                    }
                }
            } else if (imm) {
                // CMP immediate
                if (immediate9 & 0x100) {
                    int16_t signedImm = immediate9 | 0xFE00;
                    oss << "cmp  " << getRegisterName(rX) << ", #-0x" << std::hex << (-signedImm);
                } else {
                    oss << "cmp  " << getRegisterName(rX) << ", #0x" << std::hex << immediate9;
                }
            } else {
                oss << "cmp  " << getRegisterName(rX) << ", " << getRegisterName(rY);
            }
        }
        break;
    }
    
    return oss.str();
}

inline std::string formatDataWord(uint16_t value) {
    std::ostringstream oss;
    oss << "data 0x" << std::hex << std::setw(4) << std::setfill('0') << value;
    if (value >= 0x20 && value < 0x7F) {
        oss << " '" << static_cast<char>(value) << "'";
    }
    return oss.str();
}

// FORMAT HELPERS

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
        case InstrFormat::NO_OPERAND:     return "(none)";
        default:                          return "unknown";
    }
}

inline std::string getFormatHint(const InstructionDef* def) {
    if (!def) return "";
    return "Expected format: " + def->mnemonic + " " + getFormatString(def->format);
}

// DOCUMENTATION GENERATOR

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
            instr.mnemonic == "and" || instr.mnemonic == "cmp") {
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

    doc << "\n## Control\n\n";
    doc << "| Mnemonic | Format | Description |\n";
    doc << "|----------|--------|-------------|\n";
    for (const auto& instr : getInstructionTable()) {
        if (instr.format == InstrFormat::NO_OPERAND) {
            doc << "| `" << instr.mnemonic << "` | " << getFormatString(instr.format) 
                << " | " << instr.description << " |\n";
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

    doc << "\n## Memory Map (DE10-Lite)\n\n";
    doc << "| Address | Description |\n";
    doc << "|---------|-------------|\n";
    doc << "| `0x0000 - 0x03FF` | Program/Data Memory (1024 words) |\n";
    doc << "| `0x0064` | ISR Entry Point (Timer Interrupt) |\n";
    doc << "| `0x1000` | LED Output Register |\n";
    doc << "| `0x2000 - 0x2005` | 7-Segment Display (6 digits) |\n";
    doc << "| `0x3000` | Switch Input Register (read-only) |\n";
    doc << "| `0x4000` | Timer Data Low Word |\n";
    doc << "| `0x4001` | Timer Data High Word |\n";
    doc << "| `0x5000` | Timer Control (1=INIT, 2=START, 3=ACK) |\n";
    
    return doc.str();
}