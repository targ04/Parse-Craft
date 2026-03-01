/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


#ifndef PARSER_DEF_H
#define PARSER_DEF_H

#include <stdbool.h>
#include <stdio.h>
#include "grammarDef.h"

typedef struct {
    int prodIdx;   // index in G->prods
    int altIdx;    // which alternative for that LHS
} PTEntry;

typedef struct {
    int nRows;          // #nonterminals
    int nCols;          // #terminals excluding EPS

    // row/col -> symbolId
    int *ntSymbolIdByRow;   // row -> nonterminal symbolID
    int *tSymbolIdByCol;    // col -> terminal symbolID (not EPS)

    // symbolId -> row/col
    int *rowOfSymbolId;     // symbolID -> row or -1
    int *colOfSymbolId;     // symbolID -> col or -1

    PTEntry *cell;          // nRows*nCols
} ParseTable;

typedef struct {
    int numConflicts;
    int numFilled;
} ParseTableStats;

/* Parse Tree (defined inside parser module)  */

typedef struct ParseTreeNode {
    int symbolId;                    // grammar symbol id
    bool isTerminal;                 // cached
    tokenInfo tk;                    // valid iff terminal matched; for EPS/$ you can ignore
    bool hasToken;                   // whether tk is meaningful

    struct ParseTreeNode *parent;
    struct ParseTreeNode *firstChild;
    struct ParseTreeNode *nextSibling;
} ParseTreeNode;

#endif