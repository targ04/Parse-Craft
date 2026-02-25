#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdio.h>
#include "grammarDef.h"

// Create/Destroy
void initGrammar(Grammar *G);
void freeGrammar(Grammar *G);

// I/O
// Reads grammar.txt lines like: <A> ===> x y | EPS
// Returns 1 on success, 0 on failure.
int readGrammar(Grammar *G, const char *filename);

// Debug print
void printGrammar(const Grammar *G, FILE *out);

// Symbol helpers
int getOrAddSymbol(Grammar *G, const char *name);        // creates if absent
int findSymbolId(const Grammar *G, const char *name);    // -1 if missing
bool isNonTerminalName(const char *s);                   // "<...>"
bool isEpsilonName(const char *s);                       // "EPS"
bool isDollarName(const char *s);                        // "$"
bool isTerminalId(const Grammar *G, int symId);
bool isNonTerminalId(const Grammar *G, int symId);

#endif