// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Main program for FLEX & Bison assembler
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

void writeMIF(const std::vector<uint16_t>& machineCode,
              const std::vector<bool>& isData,
              std::string& outputFile,
              int depth = 256);

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " input_file [options]\n"
              << "Assemble qCore assembly to MIF format\n\n"
              << "Options:\n"
              << " -o <file>, --output <file>              Specify output file (default: a.mif)\n"
              << " -v, --verbose                           Enable verbose output\n"
              << " --doc                                   Generate instruction set documentation\n"
              << " -h, --help                              Display this help message\n"; 
}

int main(int argc, const char* argv[]) {
    std::string outputFile = "a.mif";
    bool verbose = false;
    std::string inputFile;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "-h" || arg == "--help") {
            printHelp("sbasm");
            return 0;
        }
        if(arg == "--doc") {
            // Generate instruction set documentation
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

    // Open input file for the lexer
    yyin = fopen(inputFile.c_str(), "r");
    if (yyin == nullptr) {
        std::cerr << "Error: Could not open file '" << inputFile << "'" << std::endl;
        return 1;
    }

    int memoryDepth = 256;
    
    // Parse the memory depth from DEPTH directive if present
    std::ifstream file(inputFile);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("DEPTH") != std::string::npos) {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string depthStr = line.substr(pos + 1);
                    memoryDepth = std::stoi(depthStr);
                }
            }
        }
        file.close();
    }

    try {
        if (verbose) {
            std::cout << "\n=== Lexical Analysis & Parsing (FLEX & Bison) ===\n";
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

        if (verbose) {
            std::cout << "Abstract Syntax Tree:\n";
            for (const auto& stmt : ast) {
                std::cout << "Line " << stmt->line << ", Col " << stmt->column << ": ";
                switch(stmt->type) {
                    case StatementType::LABEL: {
                        auto label = static_cast<Label*>(stmt.get());
                        std::cout << "LABEL \"" << label->name << "\"\n";
                        break;
                    }
                    case StatementType::DIRECTIVE: {
                        auto directive = static_cast<Directive*>(stmt.get());
                        std::cout << "DIRECTIVE " << directive->name;
                        if (!directive->label.empty()) 
                            std::cout << " " << directive->label;
                        if (!directive->value.empty()) 
                            std::cout << " " << directive->value;
                        std::cout << "\n";
                        break;
                    }
                    case StatementType::INSTRUCTION: {
                        auto instr = static_cast<Instruction*>(stmt.get());
                        std::cout << "INSTRUCTION " << instr->opcode;
                        if (!instr->operand1.empty()) 
                            std::cout << " " << instr->operand1;
                        if (!instr->operand2.empty()) 
                            std::cout << " " << instr->operand2;
                        std::cout << "\n";
                        break;
                    }
                }
            }
        }

        SymbolTable symbolTable;
        int currentAddress = 0;
        
        std::vector<bool> isData;

        if (verbose) {
            std::cout << "\n=== First Pass: Symbol Collection ===\n";
        }

        // Helper function to parse numeric values
        auto parseNumber = [](const std::string& valStr) -> int64_t {
            if (valStr.size() >= 2 && (valStr.substr(0, 2) == "0b" || valStr.substr(0, 2) == "0B")) {
                return std::stoll(valStr.substr(2), nullptr, 2);
            }
            else if (valStr.size() >= 2 && (valStr.substr(0, 2) == "0x" || valStr.substr(0, 2) == "0X")) {
                return std::stoll(valStr.substr(2), nullptr, 16);
            }
            else {
                return std::stoll(valStr, nullptr, 0);
            }
        };

        for(const auto& stmt : ast) {
            switch(stmt->type) {
                case StatementType::LABEL: {
                    auto label = static_cast<Label*>(stmt.get());
                    if (verbose) {
                        std::cout << "Adding label: " << label->name << " at address 0x" 
                                 << std::hex << currentAddress << std::dec << "\n";
                    }
                    symbolTable.addLabel(label->name, currentAddress);
                    break;
                }
               case StatementType::DIRECTIVE: {
                    auto directive = static_cast<Directive*>(stmt.get());
                    if(directive->name == ".define") {
                        int64_t value = parseNumber(directive->value);

                        if (verbose) {
                            std::cout << "Adding define: " << directive->label << " = 0x" 
                                    << std::hex << value << std::dec << "\n";
                        }
                        symbolTable.addDefine(directive->label, value);
                    } else if(directive->name == ".org") {
                        int64_t targetAddr = parseNumber(directive->value);
                        
                        if (targetAddr < currentAddress) {
                            throw std::runtime_error("Error at line " + std::to_string(directive->line) + 
                                                    ": .org address 0x" + std::to_string(targetAddr) + 
                                                    " is less than current address 0x" + std::to_string(currentAddress));
                        }
                        
                        if (verbose) {
                            std::cout << ".org directive: moving from 0x" 
                                    << std::hex << currentAddress << " to 0x" << targetAddr << std::dec << "\n";
                        }
                        
                        // Mark padding as data
                        while (currentAddress < targetAddr) {
                            isData.push_back(true);  // Padding is treated as data
                            currentAddress++;
                        }
                    } else if(directive->name == ".word") {
                        if (verbose) {
                            std::cout << "Word directive at address 0x" 
                                    << std::hex << currentAddress << std::dec << "\n";
                        }
                        currentAddress++;
                        isData.push_back(true);
                    }
                    break;
                } 
                case StatementType::INSTRUCTION: {
                    auto instr = static_cast<Instruction*>(stmt.get());
                    int numWords = 1;
                    
                    // Check if this is a mv instruction with =label (expands to 2 words)
                    if (instr->isLabelImmediate) {
                        const InstructionDef* def = getInstructionDef(instr->opcode);
                        if (def && def->canExpand) {
                            numWords = 2;
                        }
                    }

                    if (verbose) {
                        std::cout << "Instruction '" << instr->opcode << "' at address 0x" 
                                    << std::hex << currentAddress << std::dec 
                                    << " (size=" << numWords << ")\n";
                    }

                    currentAddress += numWords;

                    for(int i = 0; i < numWords; i++) {
                        isData.push_back(false);
                    }
                    break;
                }
            }
        }

        Encoder encoder(symbolTable);
        std::vector<uint16_t> machineCode = encoder.encode(ast);

        if (verbose) {
            std::cout << "\n=== Final Machine Code ===\n";
            for (size_t i = 0; i < machineCode.size(); i++) {
                std::cout << " " << std::hex << std::setw(3) << std::setfill('0') << i 
                         << ":  " << std::setw(4) << std::setfill('0') 
                         << machineCode[i] << std::dec << "\n";
            }
        }

        writeMIF(machineCode, isData, outputFile, memoryDepth);
        std::cout << "\nAssembly completed successfully. Output written to " << outputFile << "\n";

        // Clean up
        delete g_ast;

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void writeMIF(const std::vector<uint16_t>& machineCode,
    const std::vector<bool>& isData,
    std::string& outputFile,
    int depth) {

    if(outputFile.size() < 4 || outputFile.substr(outputFile.size() - 4) != ".mif") {
        outputFile += ".mif";
    }

    std::ofstream out(outputFile);
        if (!out.is_open()) {
        throw std::runtime_error("Could not open output file: " + outputFile);
    }

    out << "WIDTH = 16;\n";
    out << "DEPTH = " << depth << ";\n";
    out << "ADDRESS_RADIX = HEX;\n";
    out << "DATA_RADIX = HEX;\n\n";
    out << "CONTENT\n";
    out << "BEGIN\n";

    const char* regNames[] = {"r0", "r1", "r2", "r3", "r4", "sp", "lr", "pc"};
    const char* conditions[] = {"b   ", "beq ", "bne ", "bcc ", "bcs ", "bpl ", "bmi ", "bl  "};

    for (size_t i = 0; i < machineCode.size(); i++) {
        out << std::hex << std::setw(1) << i;
        out << std::string(i > 0xf ? 6 : 7, ' ');

        out << ": " << std::setw(4) << std::setfill('0') << machineCode[i] << ";"
        << std::string(8, ' ') << "% ";

        if (i < isData.size() && isData[i]) {
        out << "data";
    } else {
        uint16_t instr = machineCode[i];
        uint16_t opcode = (instr >> 13) & 0x7;
        uint16_t imm = (instr >> 12) & 0x1;
        uint16_t rX = (instr >> 9) & 0x7;
        uint16_t rY = instr & 0x7;
        uint16_t immediate = instr & 0x1FF;
        
        std::ostringstream oss;
        switch (opcode) {
            case 0:
                if (imm) {
                    oss << "mv   " << regNames[rX] << ", #0x" << std::hex << immediate;
                } else {
                    oss << "mv   " << regNames[rX] << ", " << regNames[rY];
                }
                break;

            case 1:
                if (imm) {
                    oss << "mvt  " << regNames[rX] << ", #0x" << std::hex << (immediate & 0xFF);
                } else {
                    uint16_t cond = (instr >> 9) & 0x7;
                    int16_t offset = immediate;
                    if (offset & 0x100) {
                        offset |= 0xFF00;
                    }
                    int target = i + 1 + offset;
                    oss << conditions[cond] << "0x" << std::hex << target;
                }
                break;

            case 2:
                if (imm) {
                    oss << "add  " << regNames[rX] << ", #0x" << std::hex << immediate;
                } else {
                    oss << "add  " << regNames[rX] << ", " << regNames[rY];
                }
                break;

            case 3:
                if (imm) {
                    oss << "sub  " << regNames[rX] << ", #0x" << std::hex << immediate;
                } else {
                    oss << "sub  " << regNames[rX] << ", " << regNames[rY];
                }
                break;

            case 4:
                if (imm) {
                    oss << "pop  " << regNames[rX];
                } else {
                    oss << "ld   " << regNames[rX] << ", [" << regNames[rY] << "]";
                }
                break;

            case 5:
                if (imm) {
                    oss << "push " << regNames[rX];
                } else {
                    oss << "st   " << regNames[rX] << ", [" << regNames[rY] << "]";
                }
                break;

            case 6:
                if (imm) {
                    oss << "and  " << regNames[rX] << ", #0x" << std::hex << immediate;
                } else {
                    oss << "and  " << regNames[rX] << ", " << regNames[rY];
                }
                break;

                case 7:
                {
                    uint16_t shift_flag = (instr >> 8) & 0x1;
                    
                    if (shift_flag) {
                        uint16_t imm_shift = (instr >> 7) & 0x1;
                        uint16_t shift_type = (instr >> 5) & 0x3;
                        uint16_t shift_amount = instr & 0xF;
                        const char* shift_types[] = {"lsl", "lsr", "asr", "ror"};
                        
                        // Check for HALT: 1110---11111----
                        if (imm_shift && shift_type == 3 && (instr & 0x10)) {
                            oss << "halt";
                        } else {
                            oss << shift_types[shift_type] << "  " << regNames[rX];
                            if (imm_shift) {
                                oss << ", #0x" << std::hex << shift_amount;
                            } else {
                                oss << ", " << regNames[rY];
                            }
                        }
                    } else if (imm) {
                        if (immediate & 0x100) {
                            int16_t signedImm = immediate | 0xFE00;
                            oss << "cmp  " << regNames[rX] << ", #-0x" << std::hex << (-signedImm);
                        } else {
                            oss << "cmp  " << regNames[rX] << ", #0x" << std::hex << immediate;
                        }
                    } else {
                        oss << "cmp  " << regNames[rX] << ", " << regNames[rY];
                    }
                }
                break;
        }
            out << oss.str();
        }
            out << " %\n";
        }

        if(machineCode.size() < static_cast<size_t>(depth)) {
        out << "[" << std::hex << machineCode.size() << ".." << (depth - 1) << "]" << " : 0000;\n";
    }

    out << "END;\n";
    out.close();
}