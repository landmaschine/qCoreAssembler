# SBASM - qCore Assembler (FLEX & Bison Implementation)

This is an alternative implementation of the qCore assembler using FLEX (lexical analyzer generator) and Bison (parser generator).

## Overview

This project is a FLEX & Bison-based implementation of the qCore assembler, providing the same functionality as the original C++ hand-written lexer and parser version, but using standard compiler-compiler tools.

## Features

- **FLEX Lexer**: Tokenizes assembly source code
- **Bison Parser**: Parses tokens into an Abstract Syntax Tree (AST)
- **Instruction Encoding**: Converts AST to machine code
- **MIF Output**: Generates Memory Initialization Files

## Prerequisites

- CMake 3.18 or higher
- FLEX (Fast Lexical Analyzer Generator)
- Bison (GNU Parser Generator)
- C++14 compatible compiler (GCC, Clang, or MSVC)

### Installing Dependencies

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install flex bison build-essential cmake
```

#### macOS
```bash
brew install flex bison cmake
```

#### Windows
- Download MSYS2 from https://www.msys2.org/
- Install FLEX, Bison, and CMake via pacman

## Building

```bash
mkdir build
cd build
cmake ..
make
```

The compiled executable will be in the `bin/` directory.

## Usage

```bash
./bin/sbasm <input.asm> [options]

Options:
  -o <file>, --output <file>   Specify output MIF file (default: a.mif)
  -v, --verbose                Enable verbose output
  -h, --help                   Display help message
```

### Example

```bash
./bin/sbasm program.asm -o output.mif -v
```

## Assembly Language Syntax

### Instructions

Supported instruction types:
- **Move**: `mv r1, r2` or `mv r1, #10` or `mv r1, =LABEL`
- **Arithmetic**: `add r1, r2` | `sub r1, r2`
- **Logical**: `and r1, r2` | `xor r1, r2`
- **Compare**: `cmp r1, r2`
- **Shift**: `lsl r1, #5` | `lsr r1, r2` | `asr r1, #3` | `ror r1, r2`
- **Memory**: `ld r1, [r2]` | `st r1, [r2]`
- **Stack**: `push r1` | `pop r1`
- **Branches**: `b LABEL` | `beq LABEL` | `bne LABEL` | `bcc LABEL` | `bcs LABEL` | `bpl LABEL` | `bmi LABEL` | `bl LABEL`
- **Top Register**: `mvt r1, #0xFF`

### Directives

- **`.word <value>`**: Allocate a word of data
- **`.define <name> <value>`**: Define a constant symbol

### Labels

```assembly
loop_start:
    add r1, #1
    beq loop_start
```

### Comments

```assembly
// This is a comment
mv r1, r2  // Inline comment
```

## Project Structure

```
flex-bison/
├── lexer.l              # FLEX lexer specification
├── parser.y             # Bison parser specification
├── main.cpp             # Main program
├── ast.h                # AST node definitions
├── common.h             # Common includes and utilities
├── InstructionEncoder.h # Encoder header
├── InstructionEncoder.cpp # Encoder implementation
├── SymbolTable.h        # Symbol table for labels and defines
├── CMakeLists.txt       # CMake build configuration
└── README.md            # This file
```

## Differences from Hand-Written Version

The FLEX & Bison version provides the same functionality as the original assembler but with:
- **Standard Tools**: Uses industry-standard lexer and parser generators
- **Maintainability**: Grammar rules are more declarative and easier to modify
- **Reliability**: Well-tested FLEX and Bison implementations reduce errors
- **Extensibility**: Grammar modifications are simpler and cleaner

## Building with Verbose Output

To see detailed parsing information:

```bash
./bin/sbasm program.asm -v
```

This will display:
- Tokenization details
- Abstract Syntax Tree structure
- Symbol collection information
- Final machine code generation

## License

Same as the original sbasmCpp project.

## Author

Implementation using FLEX & Bison by LeonW (February 2025)

## References

- [FLEX Manual](https://westes.github.io/flex/manual/)
- [Bison Manual](https://www.gnu.org/software/bison/manual/)
- Original sbasmCpp project
