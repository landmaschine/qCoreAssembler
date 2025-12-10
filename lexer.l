%{
// ============================================================================
// Author: LeonW
// Date: December 10, 2025
// Description: FLEX lexer specification for the qCore assembler
//              Uses table-driven lookup from InstructionDef.h for instructions,
//              registers, and directives - no hardcoding needed!
// ============================================================================

#include "parser.h"
#include "common.h"
#include "InstructionDef.h"
#include <string>
#include <iostream>
#include <cstdlib>
#include <cctype>

int line_num = 1;
int col_num = 1;

void update_location() {
    col_num += yyleng;
}

%}

%option yylineno
%option noyywrap
%option never-interactive

DIGIT       [0-9]
HEX_DIGIT   [0-9a-fA-F]
LETTER      [a-zA-Z_]
IDENT       [a-zA-Z_][a-zA-Z0-9_]*

%%

"//".*                      { /* Skip comments */ }
[ \t\r]+                    { col_num += yyleng; }
\n                          { line_num++; col_num = 1; }

"."{IDENT}                  {
                                update_location();
                                // Check if it's a valid directive from the table
                                if (isValidDirective(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return DIRECTIVE;
                                }
                                // Unknown directive - report error
                                fprintf(stderr, "Unknown directive '%s' at line %d, column %d\n", yytext, line_num, col_num);
                                return INVALID;
                            }

{IDENT}:                    { update_location(); yytext[yyleng-1] = '\0'; yylval.str = strdup(yytext); return LABEL; }

#-?{DIGIT}+                 { update_location(); yylval.str = strdup(yytext+1); return NUMBER_IMMEDIATE; }
#0[xX]{HEX_DIGIT}+          { update_location(); yylval.str = strdup(yytext+1); return NUMBER_IMMEDIATE; }
#0[bB][01]+                 { update_location(); yylval.str = strdup(yytext+1); return NUMBER_IMMEDIATE; }
#{IDENT}                    { update_location(); yylval.str = strdup(yytext+1); return NUMBER_IMMEDIATE; }

"="-?{DIGIT}+               { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="0[xX]{HEX_DIGIT}+        { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="0[bB][01]+               { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="{IDENT}                  { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }

-?{DIGIT}+                  { update_location(); yylval.str = strdup(yytext); return NUMBER; }
0[xX]{HEX_DIGIT}+           { update_location(); yylval.str = strdup(yytext); return NUMBER; }
0[bB][01]+                  { update_location(); yylval.str = strdup(yytext); return NUMBER; }

{IDENT}                     { 
                                update_location();
                                // Priority order: instruction > register > label reference
                                if (isValidInstruction(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return INSTRUCTION;
                                }
                                if (isValidRegister(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return REGISTER;
                                }
                                yylval.str = strdup(yytext);
                                return LABEL_REF;
                            }

","                         { update_location(); return COMMA; }
"["                         { update_location(); return BRACKET_OPEN; }
"]"                         { update_location(); return BRACKET_CLOSE; }

.                           { fprintf(stderr, "Unexpected character '%c' at line %d, column %d\n", yytext[0], line_num, col_num); return INVALID; }

%%
