// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Table-driven instruction encoder implementation
//              Uses InstructionDef.h for instruction definitions
// ============================================================================

#include "InstructionEncoder.h"

uint8_t Encoder::parseRegister(const std::string& reg) {
    auto& regMap = getRegisterMap();
    auto it = regMap.find(reg);
    if (it == regMap.end()) {
        throw std::runtime_error("Invalid register name: " + reg);
    }
    return it->second;
}

uint16_t Encoder::encodeImmediate(int64_t value, int bits, const std::string& context) {
    int64_t maxVal = (1ll << (bits - 1)) - 1;
    int64_t minVal = -(1ll << (bits - 1));
    
    if (value > maxVal || value < minVal) {
        throw std::runtime_error("Immediate value " + std::to_string(value) + 
                                " out of range [" + std::to_string(minVal) + ", " + 
                                std::to_string(maxVal) + "] for " + context);
    }
    
    if (value < 0) 
        value += (1 << bits);
    return static_cast<uint16_t>(value & ((1 << bits) - 1));
}

int64_t Encoder::parseImmediateOrSymbol(const std::string& value, const std::string& context) {
    try {
        if (symbolTable.hasDefine(value)) {
            return symbolTable.getDefineValue(value);
        }
        
        std::string numStr = value;
        // Strip # or = prefix if present
        if (!numStr.empty() && (numStr[0] == '#' || numStr[0] == '=')) numStr = numStr.substr(1);
        
        // Check if it's a symbol after stripping prefix
        if (symbolTable.hasDefine(numStr)) {
            return symbolTable.getDefineValue(numStr);
        }
        if (symbolTable.hasLabel(numStr)) {
            return symbolTable.getLabelAddress(numStr);
        }
        
        if (numStr.size() >= 2) {
            if (numStr.substr(0, 2) == "0x" || numStr.substr(0, 2) == "0X") {
                return std::stoll(numStr.substr(2), nullptr, 16);
            }
            if (numStr.substr(0, 2) == "0b" || numStr.substr(0, 2) == "0B") {
                return std::stoll(numStr.substr(2), nullptr, 2);
            }
        }
        return std::stoll(numStr, nullptr, 0);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse immediate value '" + value + "' for " + context + ": " + e.what());
    }
}

// ============================================================================
// Generic encoding functions - one per instruction format
// ============================================================================

void Encoder::encodeRegReg(const InstructionDef* def, uint8_t rX, uint8_t rY) {
    machineCode.push_back(def->opcodeReg | (rX << 9) | rY);
    currentAddress++;
}

void Encoder::encodeRegImm(const InstructionDef* def, uint8_t rX, int64_t imm) {
    machineCode.push_back(def->opcodeImm | (rX << 9) | encodeImmediate(imm, def->immBits, def->mnemonic));
    currentAddress++;
}

void Encoder::encodeRegImmOrReg(const InstructionDef* def, Instruction* instr, uint8_t rX) {
    // Handle label immediate (=label) - generates MVT + instruction sequence
    if (instr->isLabelImmediate) {
        int64_t value;
        if (symbolTable.hasLabel(instr->operand2)) {
            value = symbolTable.getLabelAddress(instr->operand2);
        } else if (symbolTable.hasDefine(instr->operand2)) {
            value = symbolTable.getDefineValue(instr->operand2);
        } else {
            value = parseImmediateOrSymbol(instr->operand2, def->mnemonic + " label immediate");
        }
        
        // For mv instruction with =label, generate MVT + ADD sequence
        if (def->mnemonic == "mv") {
            // MVT rX, high byte
            auto mvtDef = getInstructionDef("mvt");
            machineCode.push_back(mvtDef->opcodeImm | (rX << 9) | ((value >> 8) & 0xFF));
            // ADD rX, low byte
            auto addDef = getInstructionDef("add");
            machineCode.push_back(addDef->opcodeImm | (rX << 9) | (value & 0xFF));
            currentAddress += 2;
        } else {
            // For ALU ops with =label, generate MVT + op sequence
            auto mvtDef = getInstructionDef("mvt");
            machineCode.push_back(mvtDef->opcodeImm | (rX << 9) | ((value >> 8) & 0xFF));
            machineCode.push_back(def->opcodeImm | (rX << 9) | (value & 0xFF));
            currentAddress += 2;
        }
        return;
    }
    
    // Handle regular immediate (#value)
    if (instr->isImmediate) {
        int64_t value;
        if (symbolTable.hasDefine(instr->operand2)) {
            value = symbolTable.getDefineValue(instr->operand2);
        } else {
            value = parseImmediateOrSymbol(instr->operand2, def->mnemonic);
        }
        
        int64_t maxVal = (1ll << (def->immBits - 1)) - 1;
        int64_t minVal = -(1ll << (def->immBits - 1));
        if (value > maxVal || value < minVal) {
            throw std::runtime_error("Immediate value with # must fit in " + 
                                    std::to_string(def->immBits) + " bits, got: " + 
                                    std::to_string(value) + ". Use = for larger values.");
        }
        
        encodeRegImm(def, rX, value);
    } else {
        // Register operand
        uint8_t rY = parseRegister(instr->operand2);
        encodeRegReg(def, rX, rY);
    }
}

void Encoder::encodeBranch(const InstructionDef* def, Instruction* instr) {
    const int targetAddr = symbolTable.getLabelAddress(instr->operand1);
    const int offset = targetAddr - (currentAddress + 1);
    
    if (offset > 255 || offset < -256) {
        throw std::runtime_error("Branch target too far (offset " + std::to_string(offset) + " words)");
    }
    
    // Condition code is stored in extraData field
    machineCode.push_back(def->opcodeReg | (def->extraData << 9) | encodeImmediate(offset, def->immBits, "branch offset"));
    currentAddress++;
}

void Encoder::encodeRegOnly(const InstructionDef* def, uint8_t rX) {
    // extraData contains the second register (e.g., SP for push/pop)
    machineCode.push_back(def->opcodeReg | (rX << 9) | def->extraData);
    currentAddress++;
}

void Encoder::encodeRegMem(const InstructionDef* def, uint8_t rX, uint8_t rY) {
    machineCode.push_back(def->opcodeReg | (rX << 9) | rY);
    currentAddress++;
}

void Encoder::encodeShift(const InstructionDef* def, Instruction* instr, uint8_t rX) {
    // Shift type from extraData: 0=LSL, 1=LSR, 2=ASR, 3=ROR
    uint8_t shiftType = def->extraData;
    uint16_t encoded = def->opcodeReg | (rX << 9) | (0b10 << 7) | (shiftType << 5);
    
    if (instr->isImmediate) {
        const int64_t imm = parseImmediateOrSymbol(instr->operand2, "shift amount");
        if (imm > 15 || imm < 0) {
            throw std::runtime_error("Shift amount must be between 0 and 15");
        }
        encoded |= (1 << 7) | (imm & 0xF);
    } else {
        const uint8_t rY = parseRegister(instr->operand2);
        encoded |= rY;
    }
    
    machineCode.push_back(encoded);
    currentAddress++;
}

void Encoder::encodeLabelLoad(Instruction* instr, uint8_t rX) {
    int64_t value;
    if (symbolTable.hasLabel(instr->operand2)) {
        value = symbolTable.getLabelAddress(instr->operand2);
    } else if (symbolTable.hasDefine(instr->operand2)) {
        value = symbolTable.getDefineValue(instr->operand2);
    } else {
        value = parseImmediateOrSymbol(instr->operand2, "label load");
    }
    
    auto mvtDef = getInstructionDef("mvt");
    auto addDef = getInstructionDef("add");
    
    machineCode.push_back(mvtDef->opcodeImm | (rX << 9) | ((value >> 8) & 0xFF));
    machineCode.push_back(addDef->opcodeImm | (rX << 9) | (value & 0xFF));
    currentAddress += 2;
}

// ============================================================================
// Main encoding dispatcher - routes to appropriate format handler
// ============================================================================

void Encoder::encodeInstruction(Instruction* instr) {
    try {
        const InstructionDef* def = getInstructionDef(instr->opcode);
        if (!def) {
            throw std::runtime_error("Unknown instruction: " + instr->opcode);
        }
        
        uint8_t rX = 0;
        if (def->format != InstrFormat::BRANCH) {
            try {
                rX = parseRegister(instr->operand1);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string(e.what()) + "\n  " + getFormatHint(def));
            }
        }
        
        switch (def->format) {
            case InstrFormat::REG_REG:
                try {
                    encodeRegReg(def, rX, parseRegister(instr->operand2));
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string(e.what()) + "\n  " + getFormatHint(def));
                }
                break;
                
            case InstrFormat::REG_IMM:
                encodeRegImm(def, rX, parseImmediateOrSymbol(instr->operand2, def->mnemonic));
                break;
                
            case InstrFormat::REG_IMM_OR_REG:
                encodeRegImmOrReg(def, instr, rX);
                break;
                
            case InstrFormat::BRANCH:
                encodeBranch(def, instr);
                break;
                
            case InstrFormat::REG_ONLY:
                encodeRegOnly(def, rX);
                break;
                
            case InstrFormat::REG_MEM:
                try {
                    encodeRegMem(def, rX, parseRegister(instr->operand2));
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string(e.what()) + "\n  " + getFormatHint(def));
                }
                break;
                
            case InstrFormat::SHIFT:
                encodeShift(def, instr, rX);
                break;
                
            case InstrFormat::LABEL_LOAD:
                encodeLabelLoad(instr, rX);
                break;
                
            default:
                throw std::runtime_error("Unhandled instruction format for: " + instr->opcode + 
                                        "\n  " + getFormatHint(def));
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error at line " + std::to_string(instr->line) + ": " + e.what());
    }
}

void Encoder::encodeDirective(Directive* dir) {
    try {
        if (dir->name == ".word") {
            int64_t value = parseImmediateOrSymbol(dir->value, ".word directive");
            if (value > 0xFFFF || value < -0x8000) {
                throw std::runtime_error(".word value out of range [-32768, 65535]");
            }
            machineCode.push_back(static_cast<uint16_t>(value & 0xFFFF));
            currentAddress++;
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error encoding directive at line " + 
                                std::to_string(dir->line) + ": " + e.what());
    }
}

std::vector<uint16_t> Encoder::encode(const std::vector<std::unique_ptr<Statement>>& ast) {
    machineCode.clear();
    currentAddress = 0;
    
    for (const auto& stmt : ast) {
        switch (stmt->type) {
            case StatementType::LABEL:
                // Labels are already added in the first pass, skip here
                break;
            case StatementType::DIRECTIVE:
                encodeDirective(static_cast<Directive*>(stmt.get()));
                break;
            case StatementType::INSTRUCTION:
                encodeInstruction(static_cast<Instruction*>(stmt.get()));
                break;
            default:
                throw std::runtime_error("Unknown statement type at line " + std::to_string(stmt->line));
        }
    }
    return machineCode;
}
