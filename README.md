# Parse-Craft

Parse-Craft is a compiler construction project for **CS F363: Compiler Construction** at BITS Pilani.  
It implements the front-end modules of a small programming language:

- Lexical Analyzer
- Grammar Loader
- FIRST and FOLLOW Set Computation
- LL(1) Parse Table Construction
- Predictive Parser
- Parse Tree Generation
- Basic Panic-Mode Syntax Error Recovery

The project is written in **C** and can be built using the provided `makefile`.


## Project Scope

This repository implements the **lexical analysis** and **syntax analysis** phases of the compiler.

The language supports constructs such as:

- Primitive types: `int`, `real`
- Constructed types: `record`, `union`
- Type aliases using `definetype`
- Global and local variable declarations
- Assignment statements
- Arithmetic expressions
- Boolean expressions
- Conditional statements
- Iterative statements
- Function definitions and function calls
- Input/output statements
- Return statements

Semantic analysis, type checking, intermediate code generation, and target code generation are outside the scope of this stage.

---

## Repository Structure

```text
Parse-Craft/
├── README.md
└── compiler/
    ├── coding details.pdf
    ├── driver.c
    ├── firstfollow.c
    ├── firstfollow.h
    ├── grammar.c
    ├── grammar.h
    ├── grammar.txt
    ├── grammarDef.h
    ├── lexer.c
    ├── lexer.h
    ├── lexerDef.h
    ├── makefile
    ├── parser.c
    ├── parser.h
    ├── parserDef.h
    ├── set.c
    ├── set.h
    ├── t1.txt
    ├── t2.txt
    ├── t3.txt
    ├── t4.txt
    ├── t5.txt
    ├── t6.txt
    └── testcase1.txt