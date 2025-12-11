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

// Process escape sequences in a string
char* process_string(const char* input) {
    size_t len = strlen(input);
    char* output = (char*)malloc(len + 1);
    char* out = output;
    const char* in = input + 1;  // Skip opening quote
    
    while (*in && *in != '"') {
        if (*in == '\\' && *(in + 1)) {
            in++;
            switch (*in) {
                case 'n':  *out++ = '\n'; break;
                case 'r':  *out++ = '\r'; break;
                case 't':  *out++ = '\t'; break;
                case '\\': *out++ = '\\'; break;
                case '"':  *out++ = '"';  break;
                case '0':  *out++ = '\0'; break;
                default:   *out++ = *in;  break;
            }
        } else {
            *out++ = *in;
        }
        in++;
    }
    *out = '\0';
    return output;
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

\"([^"\\]|\\.)*\"           {
                                // Quoted string - process escape sequences
                                update_location();
                                yylval.str = process_string(yytext);
                                return STRING;
                            }

"."{IDENT}                  {
                                update_location();
                                if (isValidDirective(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return DIRECTIVE;
                                }
                                fprintf(stderr, "Unknown directive '%s' at line %d, column %d\n", yytext, line_num, col_num);
                                return INVALID;
                            }

{IDENT}:                    { update_location(); yytext[yyleng-1] = '\0'; yylval.str = strdup(yytext); return LABEL; }

#-?{DIGIT}+                 { update_location(); yylval.str = strdup(yytext+1); return IMMEDIATE; }
#0[xX]{HEX_DIGIT}+          { update_location(); yylval.str = strdup(yytext+1); return IMMEDIATE; }
#0[bB][01]+                 { update_location(); yylval.str = strdup(yytext+1); return IMMEDIATE; }
#{IDENT}                    { update_location(); yylval.str = strdup(yytext+1); return IMMEDIATE; }

"="-?{DIGIT}+               { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="0[xX]{HEX_DIGIT}+        { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="0[bB][01]+               { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }
"="{IDENT}                  { update_location(); yylval.str = strdup(yytext+1); return LABEL_IMMEDIATE; }

-?{DIGIT}+                  { update_location(); yylval.str = strdup(yytext); return NUMBER; }
0[xX]{HEX_DIGIT}+           { update_location(); yylval.str = strdup(yytext); return NUMBER; }
0[bB][01]+                  { update_location(); yylval.str = strdup(yytext); return NUMBER; }

{IDENT}                     { 
                                update_location();
                                if (isValidInstruction(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return INSTRUCTION;
                                }
                                if (isValidRegister(yytext)) {
                                    yylval.str = strdup(yytext);
                                    return REGISTER;
                                }
                                yylval.str = strdup(yytext);
                                return IDENTIFIER;
                            }

","                         { update_location(); return COMMA; }
"["                         { update_location(); return LBRACKET; }
"]"                         { update_location(); return RBRACKET; }

.                           { fprintf(stderr, "Unexpected character '%c' at line %d, column %d\n", yytext[0], line_num, col_num); return INVALID; }

%%