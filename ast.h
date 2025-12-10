// ============================================================================
// Author: LeonW
// Date: February 3, 2025
// Description: AST node definitions for FLEX & Bison parser
// ============================================================================

#pragma once
#include "common.h"
#include <memory>
#include <vector>
#include <string>

enum class StatementType {
    INSTRUCTION,
    DIRECTIVE,
    LABEL
};

class Statement {
public:
    StatementType type;
    int line;
    int column;

    Statement(StatementType t, int l, int c) : type(t), line(l), column(c) {}
    virtual ~Statement() = default;
};

class Instruction : public Statement {
public:
    std::string opcode;
    std::string operand1;
    std::string operand2;
    bool hasComma;
    bool isLabelImmediate;
    bool isImmediate;

    Instruction(const std::string& op, const std::string& op1, 
                const std::string& op2, bool comma, bool labelImm, bool imm,
                int l, int c)
        : Statement(StatementType::INSTRUCTION, l, c), 
          opcode(op), operand1(op1), operand2(op2), 
          hasComma(comma), isLabelImmediate(labelImm), isImmediate(imm) {}
};

class Directive : public Statement {
public:
    std::string name;
    std::string label;
    std::string value;

    Directive(const std::string& n, const std::string& l, 
              const std::string& v, int line, int col)
        : Statement(StatementType::DIRECTIVE, line, col), 
          name(n), label(l), value(v) {}
};

class Label : public Statement {
public:
    std::string name;

    Label(const std::string& n, int l, int c)
        : Statement(StatementType::LABEL, l, c), name(n) {}
};

// AST Node for programs (vector of statements)
using ProgramAST = std::vector<std::unique_ptr<Statement>>;

extern ProgramAST* g_ast;
