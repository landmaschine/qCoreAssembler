%{
// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: Bison parser specification for the qCore assembler
//              Clean operand handling with proper type tracking
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

// Operand type enum for clean tracking
enum class OperandType {
    NONE,
    REG,        // r0-r7, sp, lr, pc
    IMM,        // #value
    LABEL_IMM,  // =value or =LABEL
    IDENT,      // label reference or symbol
    NUMBER      // bare number
};

struct Operand {
    std::string value;
    OperandType type;
    
    Operand() : type(OperandType::NONE) {}
    Operand(const std::string& v, OperandType t) : value(v), type(t) {}
};

static std::unique_ptr<Instruction> make_instruction(
    const std::string& opcode, 
    const Operand& op1, 
    const Operand& op2,
    int line, int col) 
{
    bool isLabelImm = (op2.type == OperandType::LABEL_IMM);
    bool isImm = (op2.type == OperandType::IMM || 
                  op2.type == OperandType::LABEL_IMM || 
                  op2.type == OperandType::NUMBER);
    bool hasComma = (op2.type != OperandType::NONE);
    
    return std::make_unique<Instruction>(
        opcode, op1.value, op2.value, 
        hasComma, isLabelImm, isImm, 
        line, col
    );
}

static std::unique_ptr<Directive> make_directive(
    const std::string& name, 
    const std::string& label, 
    const std::string& value,
    int line, int col) 
{
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
    void*   operand;
}

%token <str> INSTRUCTION REGISTER NUMBER IMMEDIATE LABEL IDENTIFIER LABEL_IMMEDIATE
%token <str> DIRECTIVE STRING
%token COMMA LBRACKET RBRACKET
%token INVALID END 0

%type <node> statement instruction directive label
%type <list> program statements
%type <operand> operand

%destructor { free($$); } <str>
%destructor { delete static_cast<Operand*>($$); } <operand>

%start program

%%

program:
    statements
    {
        g_ast = new ProgramAST();
        if ($1 != nullptr) {
            auto* stmts = static_cast<std::vector<std::unique_ptr<Statement>>*>($1);
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
        auto* stmts = static_cast<std::vector<std::unique_ptr<Statement>>*>($1);
        if ($2 != nullptr) {
            stmts->push_back(std::unique_ptr<Statement>(static_cast<Statement*>($2)));
        }
        $$ = stmts;
    }
    ;

statement:
    instruction { $$ = $1; }
    | directive { $$ = $1; }
    | label     { $$ = $1; }
    ;

label:
    LABEL
    {
        $$ = make_label(std::string($1), line_num, col_num).release();
        free($1);
    }
    ;

//DIRECTIVES

directive:
    /* .word VALUE or .org VALUE or .space COUNT */
    DIRECTIVE NUMBER
    {
        $$ = make_directive($1, "", $2, line_num, col_num).release();
        free($1);
        free($2);
    }
    /* .word LABEL_REF */
    | DIRECTIVE IDENTIFIER
    {
        $$ = make_directive($1, "", $2, line_num, col_num).release();
        free($1);
        free($2);
    }
    /* .define NAME VALUE */
    | DIRECTIVE IDENTIFIER NUMBER
    {
        $$ = make_directive($1, $2, $3, line_num, col_num).release();
        free($1);
        free($2);
        free($3);
    }
    /* .ascii "string" or .asciiz "string" */
    | DIRECTIVE STRING
    {
        $$ = make_directive($1, "", $2, line_num, col_num).release();
        free($1);
        free($2);
    }
    ;

//INSTRUCTIONS

instruction:
    /* No operand: halt */
    INSTRUCTION
    {
        Operand empty;
        $$ = make_instruction($1, empty, empty, line_num, col_num).release();
        free($1);
    }
    /* Single operand - branch target: b LABEL */
    | INSTRUCTION IDENTIFIER
    {
        Operand op1($2, OperandType::IDENT);
        Operand empty;
        $$ = make_instruction($1, op1, empty, line_num, col_num).release();
        free($1);
        free($2);
    }
    /* Single operand - register: push r0, pop r1 */
    | INSTRUCTION REGISTER
    {
        Operand op1($2, OperandType::REG);
        Operand empty;
        $$ = make_instruction($1, op1, empty, line_num, col_num).release();
        free($1);
        free($2);
    }
    /* Two operands: mv r0, <operand> */
    | INSTRUCTION REGISTER COMMA operand
    {
        Operand op1($2, OperandType::REG);
        Operand* op2 = static_cast<Operand*>($4);
        $$ = make_instruction($1, op1, *op2, line_num, col_num).release();
        free($1);
        free($2);
        delete op2;
    }
    /* Memory access: ld r0, [r1] */
    | INSTRUCTION REGISTER COMMA LBRACKET REGISTER RBRACKET
    {
        Operand op1($2, OperandType::REG);
        Operand op2($5, OperandType::REG);
        $$ = make_instruction($1, op1, op2, line_num, col_num).release();
        free($1);
        free($2);
        free($5);
    }
    ;

//OPERANDS - Clean type tracking

operand:
    REGISTER
    {
        $$ = new Operand($1, OperandType::REG);
        free($1);
    }
    | IMMEDIATE
    {
        // Store with # prefix for encoder
        $$ = new Operand(std::string("#") + $1, OperandType::IMM);
        free($1);
    }
    | LABEL_IMMEDIATE
    {
        // Store with = prefix for encoder
        $$ = new Operand(std::string("=") + $1, OperandType::LABEL_IMM);
        free($1);
    }
    | IDENTIFIER
    {
        $$ = new Operand($1, OperandType::IDENT);
        free($1);
    }
    | NUMBER
    {
        $$ = new Operand($1, OperandType::NUMBER);
        free($1);
    }
    ;

%%

void yyerror(const char* msg) {
    std::cerr << "Parse error at line " << line_num << ", column " << col_num << ": " << msg << std::endl;
}