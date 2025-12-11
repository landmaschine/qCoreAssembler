// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Main program for FLEX & Bison assembler
//              Refactored MIF writer using instruction table for disassembly
// ============================================================================

#include "common.h"
#include "ast.h"
#include "InstructionEncoder.h"
#include "InstructionDef.h"
#include "SymbolTable.h"
#include <fstream>
#include <sstream>
#include <iomanip>

extern FILE* yyin;
extern int yyparse();
extern ProgramAST* g_ast;

// ============================================================================
// MIF Writer - Uses disassembly from InstructionDef.h
// ============================================================================

void writeMIF(const std::vector<uint16_t>& machineCode,
              const std::vector<bool>& isData,
              std::string& outputFile,
              int depth = 256) 
{
    if (outputFile.size() < 4 || outputFile.substr(outputFile.size() - 4) != ".mif") {
        outputFile += ".mif";
    }

    std::ofstream out(outputFile);
    if (!out.is_open()) {
        throw std::runtime_error("Could not open output file: " + outputFile);
    }

    // MIF header
    out << "WIDTH = 16;\n";
    out << "DEPTH = " << depth << ";\n";
    out << "ADDRESS_RADIX = HEX;\n";
    out << "DATA_RADIX = HEX;\n\n";
    out << "CONTENT\n";
    out << "BEGIN\n";

    // Write each word with disassembly comment
    for (size_t i = 0; i < machineCode.size(); i++) {
        // Address column
        out << std::hex << std::setw(3) << std::setfill(' ') << i;
        out << "    ";
        
        // Data column
        out << ": " << std::setw(4) << std::setfill('0') << machineCode[i] << ";";
        out << "        ";
        
        // Comment with disassembly
        out << "% ";
        if (i < isData.size() && isData[i]) {
            out << formatDataWord(machineCode[i]);
        } else {
            out << disassembleInstruction(machineCode[i], i);
        }
        out << " %\n";
    }

    // Fill remaining addresses with zeros
    if (machineCode.size() < static_cast<size_t>(depth)) {
        out << "[" << std::hex << machineCode.size() << ".." << (depth - 1) << "] : 0000;\n";
    }

    out << "END;\n";
    out.close();
}

// ============================================================================
// Help and CLI
// ============================================================================

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " input_file [options]\n"
              << "Assemble qCore assembly to MIF format\n\n"
              << "Options:\n"
              << "  -o <file>, --output <file>   Specify output file (default: a.mif)\n"
              << "  -v, --verbose                Enable verbose output\n"
              << "  --doc                        Generate instruction set documentation\n"
              << "  -h, --help                   Display this help message\n"; 
}

// ============================================================================
// Utility functions
// ============================================================================

int64_t parseNumber(const std::string& valStr) {
    if (valStr.size() >= 2 && (valStr.substr(0, 2) == "0b" || valStr.substr(0, 2) == "0B")) {
        return std::stoll(valStr.substr(2), nullptr, 2);
    }
    else if (valStr.size() >= 2 && (valStr.substr(0, 2) == "0x" || valStr.substr(0, 2) == "0X")) {
        return std::stoll(valStr.substr(2), nullptr, 16);
    }
    else {
        return std::stoll(valStr, nullptr, 0);
    }
}

// Calculate the size (in words) of a directive
int getDirectiveSize(Directive* dir) {
    if (dir->name == ".word") {
        return 1;
    } 
    else if (dir->name == ".space") {
        return static_cast<int>(parseNumber(dir->value));
    }
    else if (dir->name == ".ascii") {
        return static_cast<int>(dir->value.length());
    }
    else if (dir->name == ".asciiz") {
        return static_cast<int>(dir->value.length()) + 1;  // +1 for null terminator
    }
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, const char* argv[]) {
    std::string outputFile = "a.mif";
    bool verbose = false;
    std::string inputFile;

    // Handle --doc and --help before input file check
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp("sbasm");
            return 0;
        }
        if (arg == "--doc") {
            std::cout << generateInstructionSetDoc();
            return 0;
        }
    }

    if (argc < 2) {
        std::cerr << "Error: No input file specified.\n"
                  << "Usage: " << argv[0] << " input_file [options]\n"
                  << "Use -h for help" << std::endl;
        return 1;
    }
    inputFile = argv[1];

    // Parse remaining arguments
    for (int i = 2; i < argc; ) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an output filename" << std::endl;
                return 1;
            }
            outputFile = argv[i + 1];
            i += 2;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
            i += 1;
        } else {
            std::cerr << "Error: Unexpected argument '" << arg << "'\n"
                      << "Use -h for help" << std::endl;
            return 1;
        }
    }

    // Open input file
    yyin = fopen(inputFile.c_str(), "r");
    if (yyin == nullptr) {
        std::cerr << "Error: Could not open file '" << inputFile << "'" << std::endl;
        return 1;
    }

    int memoryDepth = 256;

    try {
        if (verbose) {
            std::cout << "\n=== Lexical Analysis & Parsing ===\n";
        }

        // Parse using Bison
        int parse_result = yyparse();
        fclose(yyin);

        if (parse_result != 0) {
            std::cerr << "Parse failed" << std::endl;
            return 1;
        }

        if (g_ast == nullptr) {
            std::cerr << "Error: No AST generated" << std::endl;
            return 1;
        }

        auto& ast = *g_ast;

        // Print AST if verbose
        if (verbose) {
            std::cout << "Abstract Syntax Tree:\n";
            for (const auto& stmt : ast) {
                std::cout << "  Line " << stmt->line << ": ";
                switch (stmt->type) {
                    case StatementType::LABEL: {
                        auto label = static_cast<Label*>(stmt.get());
                        std::cout << "LABEL \"" << label->name << "\"\n";
                        break;
                    }
                    case StatementType::DIRECTIVE: {
                        auto dir = static_cast<Directive*>(stmt.get());
                        std::cout << "DIRECTIVE " << dir->name;
                        if (!dir->label.empty()) std::cout << " " << dir->label;
                        if (!dir->value.empty()) std::cout << " \"" << dir->value << "\"";
                        std::cout << "\n";
                        break;
                    }
                    case StatementType::INSTRUCTION: {
                        auto instr = static_cast<Instruction*>(stmt.get());
                        std::cout << "INSTR " << instr->opcode;
                        if (!instr->operand1.empty()) std::cout << " " << instr->operand1;
                        if (!instr->operand2.empty()) std::cout << ", " << instr->operand2;
                        if (instr->isLabelImmediate) std::cout << " [label_imm]";
                        if (instr->isImmediate) std::cout << " [imm]";
                        std::cout << "\n";
                        break;
                    }
                }
            }
        }

        // ====================================================================
        // First Pass: Symbol Collection
        // ====================================================================
        
        SymbolTable symbolTable;
        int currentAddress = 0;
        std::vector<bool> isData;

        if (verbose) {
            std::cout << "\n=== First Pass: Symbol Collection ===\n";
        }

        for (const auto& stmt : ast) {
            switch (stmt->type) {
                case StatementType::LABEL: {
                    auto label = static_cast<Label*>(stmt.get());
                    if (verbose) {
                        std::cout << "  Label: " << label->name << " = 0x" 
                                  << std::hex << currentAddress << std::dec << "\n";
                    }
                    symbolTable.addLabel(label->name, currentAddress);
                    break;
                }
                case StatementType::DIRECTIVE: {
                    auto dir = static_cast<Directive*>(stmt.get());
                    
                    if (dir->name == ".define") {
                        int64_t value = parseNumber(dir->value);
                        if (verbose) {
                            std::cout << "  Define: " << dir->label << " = 0x" 
                                      << std::hex << value << std::dec << "\n";
                        }
                        symbolTable.addDefine(dir->label, value);
                    } 
                    else if (dir->name == ".org") {
                        int64_t targetAddr = parseNumber(dir->value);
                        if (targetAddr < currentAddress) {
                            throw std::runtime_error("Error at line " + std::to_string(dir->line) + 
                                                    ": .org address is less than current address");
                        }
                        if (verbose) {
                            std::cout << "  .org: 0x" << std::hex << currentAddress 
                                      << " -> 0x" << targetAddr << std::dec << "\n";
                        }
                        // Mark padding as data
                        while (currentAddress < targetAddr) {
                            isData.push_back(true);
                            currentAddress++;
                        }
                    }
                    else {
                        // .word, .space, .ascii, .asciiz
                        int size = getDirectiveSize(dir);
                        if (verbose) {
                            std::cout << "  " << dir->name << " at 0x" << std::hex 
                                      << currentAddress << " (size=" << std::dec << size << ")\n";
                        }
                        for (int i = 0; i < size; i++) {
                            isData.push_back(true);
                        }
                        currentAddress += size;
                    }
                    break;
                }
                case StatementType::INSTRUCTION: {
                    auto instr = static_cast<Instruction*>(stmt.get());
                    int numWords = 1;
                    
                    // Check for expanded instructions (=label generates 2 words)
                    if (instr->isLabelImmediate) {
                        const InstructionDef* def = getInstructionDef(instr->opcode);
                        if (def && def->canExpand) {
                            numWords = 2;
                        }
                    }

                    if (verbose) {
                        std::cout << "  " << instr->opcode << " at 0x" << std::hex 
                                  << currentAddress << " (size=" << std::dec << numWords << ")\n";
                    }

                    for (int i = 0; i < numWords; i++) {
                        isData.push_back(false);
                    }
                    currentAddress += numWords;
                    break;
                }
            }
        }

        // ====================================================================
        // Second Pass: Code Generation
        // ====================================================================

        if (verbose) {
            std::cout << "\n=== Second Pass: Code Generation ===\n";
        }

        Encoder encoder(symbolTable);
        std::vector<uint16_t> machineCode = encoder.encode(ast);

        if (verbose) {
            std::cout << "\nGenerated " << machineCode.size() << " words:\n";
            for (size_t i = 0; i < machineCode.size(); i++) {
                std::cout << "  " << std::hex << std::setw(3) << std::setfill('0') << i 
                          << ": " << std::setw(4) << std::setfill('0') << machineCode[i];
                if (i < isData.size() && !isData[i]) {
                    std::cout << "  ; " << disassembleInstruction(machineCode[i], i);
                }
                std::cout << std::dec << "\n";
            }
        }

        // ====================================================================
        // Write Output
        // ====================================================================

        writeMIF(machineCode, isData, outputFile, memoryDepth);
        std::cout << "\nAssembly completed. Output: " << outputFile 
                  << " (" << machineCode.size() << " words)\n";

        delete g_ast;

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}