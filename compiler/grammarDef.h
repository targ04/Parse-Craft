#ifndef GRAMMAR_DEF_H
#define GRAMMAR_DEF_H

#include <stdbool.h>

#define MAX_SYMBOL_NAME 64

// Special symbols (kept as fixed IDs for convenience everywhere)
#define SYMBOL_EPS_ID 0   // EPS
#define SYMBOL_DOLLAR_ID 1 // $

typedef enum {
    SYM_TERMINAL = 0,
    SYM_NONTERMINAL = 1
} SymbolKind;

typedef struct {
    int id;                          // 0..(numSymbols-1)
    SymbolKind kind;                 // terminal / nonterminal
    char name[MAX_SYMBOL_NAME];      // e.g. "TK_ID" or "<program>"
} Symbol;

typedef struct {
    int *rhs;        // array of symbol IDs
    int rhsLen;      // length of RHS
} RHS;

typedef struct {
    int lhs;         // symbol ID of LHS (must be nonterminal)
    RHS *alts;       // alternatives for this LHS
    int numAlts;
} Production;

typedef struct {
    // symbol registory
    Symbol *symbols;
    int numSymbols;
    int capSymbols;

    // productions (one entry per LHS nonterminal that appears in grammar.txt)
    Production *prods;
    int numProds;
    int capProds;

    int startSymbol;     // LHS of first production read

    // quick counts (optional but handy)
    int numTerminals;
    int numNonTerminals;
} Grammar;

#endif