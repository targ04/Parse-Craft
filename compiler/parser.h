/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "grammarDef.h"
#include "set.h"
#include "firstfollow.h"
// our lexer
#include "lexerDef.h"
#include "lexer.h"

#include "parserDef.h"

// Parse Table
void initParseTable(const Grammar *G, ParseTable *T);
void freeParseTable(ParseTable *T);

ParseTableStats createParseTable(
    const Grammar *G,
    const BitSet *FIRST,
    const BitSet *FOLLOW,
    ParseTable *T,
    FILE *conflictOut
);

void printParseTable(const Grammar *G, const ParseTable *T, FILE *out);

// Parser + Parse Tree 
// Returns parse tree root. If parsing fails badly, still returns partial tree (unless root alloc failed).
ParseTreeNode* parseInputSourceCode(
    twinBuffer *B,
    const Grammar *G,
    const ParseTable *T,
    const BitSet *FIRST,
    const BitSet *FOLLOW,
    FILE *out,
    bool *okOut
);

// Parse tree utilities
void printParseTree(const Grammar *G, const ParseTreeNode *root, FILE *out);
void freeParseTree(ParseTreeNode *root);

// Token mapping: token type -> grammar terminal symbol id
int terminalSymbolIdFromToken(const Grammar *G, tokenInfo tk);

#endif