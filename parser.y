%{
// ============================================================================
// Author: LeonW
// Date: February 3, 2025
// Description: Bison parser specification for the qCore assembler
// ============================================================================

#include "common.h"
#include "ast.h"
#include <memory>
#include <vector>
#include <string>
#include <iostream>

extern int yylex();
extern void yyerror(const char*);
extern int line_num;
extern int col_num;

ProgramAST* g_ast = nullptr;

static std::unique_ptr<Instruction> make_instruction(
    const std::string& opcode, const std::string& op1, const std::string& op2,
    bool hasComma, bool isLabelImm, bool isImm, int line, int col) {
    return std::make_unique<Instruction>(opcode, op1, op2, hasComma, isLabelImm, isImm, line, col);
}

static std::unique_ptr<Directive> make_directive(
    const std::string& name, const std::string& label, const std::string& value,
    int line, int col) {
    return std::make_unique<Directive>(name, label, value, line, col);
}

static std::unique_ptr<Label> make_label(const std::string& name, int line, int col) {
    return std::make_unique<Label>(name, line, col);
}

%}

%define parse.error verbose
%define parse.lac full

%union {
    char*   str;
    void*   node;
    void*   list;
}

%token INSTRUCTION REGISTER NUMBER NUMBER_IMMEDIATE LABEL LABEL_REF LABEL_IMMEDIATE
%token COMMA BRACKET_OPEN BRACKET_CLOSE DIRECTIVE COMMENT
%token INVALID END 0

%type <str> INSTRUCTION REGISTER NUMBER NUMBER_IMMEDIATE LABEL LABEL_REF LABEL_IMMEDIATE DIRECTIVE
%type <str> operand
%type <node> statement instruction directive label
%type <list> program statements

%start program

%%

program:
    statements
    {
        g_ast = new ProgramAST();
        if ($1 != nullptr) {
            auto* stmts = (std::vector<std::unique_ptr<Statement>>*)$1;
            for (auto& stmt : *stmts) {
                g_ast->push_back(std::move(stmt));
            }
            delete stmts;
        }
    }
    ;

statements:
    %empty
    {
        $$ = new std::vector<std::unique_ptr<Statement>>();
    }
    | statements statement
    {
        auto* stmts = (std::vector<std::unique_ptr<Statement>>*)$1;
        if ($2 != nullptr) {
            stmts->push_back(std::unique_ptr<Statement>((Statement*)$2));
        }
        $$ = stmts;
    }
    ;

statement:
    instruction
    {
        $$ = $1;
    }
    | directive
    {
        $$ = $1;
    }
    | label
    {
        $$ = $1;
    }
    ;

label:
    LABEL
    {
        $$ = make_label(std::string($1), line_num, col_num).release();
        free($1);
    }
    ;

directive:
    DIRECTIVE NUMBER
    {
        // .word or .org directive with numeric value
        auto dir = make_directive(std::string($1), "", std::string($2), line_num, col_num);
        $$ = dir.release();
        free($1);
        free($2);
    }
    | DIRECTIVE LABEL_REF
    {
        // .word with label reference (e.g., .word MY_LABEL)
        auto dir = make_directive(std::string($1), "", std::string($2), line_num, col_num);
        $$ = dir.release();
        free($1);
        free($2);
    }
    | DIRECTIVE LABEL_REF NUMBER
    {
        // .define directive (e.g., .define MY_CONST 42)
        auto dir = make_directive(std::string($1), std::string($2), std::string($3), line_num, col_num);
        $$ = dir.release();
        free($1);
        free($2);
        free($3);
    }
    ;

instruction:
    INSTRUCTION
    {
        // No-operand instructions (halt)
        auto instr = make_instruction(std::string($1), "", "", false, false, false, line_num, col_num);
        $$ = instr.release();
        free($1);
    }
    | INSTRUCTION LABEL_REF
    {
        // Branch instructions (b, beq, bne, bcc, bcs, bpl, bmi, bl)
        auto instr = make_instruction(std::string($1), std::string($2), "", false, false, false, line_num, col_num);
        $$ = instr.release();
        free($1);
        free($2);
    }
    | INSTRUCTION REGISTER
    {
        // push, pop instructions
        auto instr = make_instruction(std::string($1), std::string($2), "", false, false, false, line_num, col_num);
        $$ = instr.release();
        free($1);
        free($2);
    }
    | INSTRUCTION REGISTER COMMA operand
    {
        // Two operand instructions with various operand types
        auto instr = make_instruction(std::string($1), std::string($2), std::string((char*)$4), true, 
                                     /* isLabelImm determined by context */(std::string((char*)$4).find('=') != std::string::npos ? 1 : 0),
                                     /* isImmediate */(std::string((char*)$4).find('#') != std::string::npos || 
                                                       std::string((char*)$4).find('=') != std::string::npos ||
                                                       std::isdigit(((char*)$4)[0]) ? 1 : 0),
                                     line_num, col_num);
        $$ = instr.release();
        free($1);
        free($2);
        free((char*)$4);
    }
    | INSTRUCTION REGISTER COMMA BRACKET_OPEN REGISTER BRACKET_CLOSE
    {
        // Load/Store instructions: ld/st r1, [r2]
        auto instr = make_instruction(std::string($1), std::string($2), std::string($5), true, false, false, line_num, col_num);
        $$ = instr.release();
        free($1);
        free($2);
        free($5);
    }
    ;

operand:
    REGISTER
    {
        $$ = strdup($1);
    }
    | NUMBER
    {
        $$ = strdup($1);
    }
    | NUMBER_IMMEDIATE
    {
        char* buf = (char*)malloc(strlen($1) + 2);
        strcpy(buf, "#");
        strcat(buf, $1);
        $$ = buf;
    }
    | LABEL_IMMEDIATE
    {
        char* buf = (char*)malloc(strlen($1) + 2);
        strcpy(buf, "=");
        strcat(buf, $1);
        $$ = buf;
    }
    | LABEL_REF
    {
        $$ = strdup($1);
    }
    ;

%%

void yyerror(const char* msg) {
    std::cerr << "Parse error at line " << line_num << ", column " << col_num << ": " << msg << std::endl;
}
