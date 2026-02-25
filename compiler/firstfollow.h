#ifndef FIRSTFOLLOW_H
#define FIRSTFOLLOW_H

#include <stdio.h>
#include "grammarDef.h"
#include "set.h"

// FIRST and FOLLOW are arrays of BitSet of size G->numSymbols.
// Convention:
//  - FIRST[X] contains terminal symbol IDs and possibly EPS (SYMBOL_EPS_ID)
//  - FOLLOW[A] (A is nonterminal) contains terminal IDs and possibly $ (SYMBOL_DOLLAR_ID)
//  - FOLLOW should NOT contain EPS.

void initFirstFollowArrays(const Grammar *G, BitSet *FIRST, BitSet *FOLLOW);
void freeFirstFollowArrays(const Grammar *G, BitSet *FIRST, BitSet *FOLLOW);

bool firstOfSequence_noeps(
    const BitSet *FIRST,
    const RHS *r,
    int pos,
    BitSet *outNoEps
);

void computeFIRST(const Grammar *G, BitSet *FIRST);
void computeFOLLOW(const Grammar *G, const BitSet *FIRST, BitSet *FOLLOW);


void printFIRST(const Grammar *G, const BitSet *FIRST, FILE *out);
void printFOLLOW(const Grammar *G, const BitSet *FOLLOW, FILE *out);

#endif