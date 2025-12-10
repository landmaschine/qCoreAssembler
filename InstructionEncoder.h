// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Instruction encoder header - Table-driven encoding
// ============================================================================

#pragma once
#include <unordered_map>
#include <vector>
#include "SymbolTable.h"
#include "InstructionDef.h"
#include "ast.h"

class Encoder {
private:
    SymbolTable& symbolTable;
    std::vector<uint16_t> machineCode;
    int currentAddress;

    // Parse register name to register number
    uint8_t parseRegister(const std::string& reg);
    
    // Encode immediate value with range checking
    uint16_t encodeImmediate(int64_t value, int bits, const std::string& context);
    
    // Parse immediate value or symbol reference
    int64_t parseImmediateOrSymbol(const std::string& value, const std::string& context);

    // Generic encoding functions for each instruction format
    void encodeRegReg(const InstructionDef* def, uint8_t rX, uint8_t rY);
    void encodeRegImm(const InstructionDef* def, uint8_t rX, int64_t imm);
    void encodeRegImmOrReg(const InstructionDef* def, Instruction* instr, uint8_t rX);
    void encodeBranch(const InstructionDef* def, Instruction* instr);
    void encodeRegOnly(const InstructionDef* def, uint8_t rX);
    void encodeRegMem(const InstructionDef* def, uint8_t rX, uint8_t rY);
    void encodeShift(const InstructionDef* def, Instruction* instr, uint8_t rX);
    void encodeLabelLoad(Instruction* instr, uint8_t rX);
    
    // Main encoding dispatcher
    void encodeInstruction(Instruction* instr);
    void encodeDirective(Directive* dir);

public:
    Encoder(SymbolTable& st) : symbolTable(st), currentAddress(0) {}

    std::vector<uint16_t> encode(const std::vector<std::unique_ptr<Statement>>& ast);
};
