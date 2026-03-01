/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/



#include "firstfollow.h"
#include <stdbool.h>

//  helpers

static bool addSetExceptEps(BitSet *dst, const BitSet *src) {
    bool changed = false;
    for (int x = setNext(src, 0); x != -1; x = setNext(src, x + 1)) {
        if (x == SYMBOL_EPS_ID) continue;
        changed |= setAdd(dst, x);
    }
    return changed;
}

// FIRST(beta) for a sequence beta = rhs[pos..end)
// Adds FIRST(beta) \ {EPS} into outNoEps.
// Returns true if beta is nullable (i.e., FIRST(beta) contains EPS)
bool firstOfSequence_noeps(
    const BitSet *FIRST,
    const RHS *r,
    int pos, 
    BitSet *outNoEps
) {
    // Empty suffix => nullable
    if (pos >= r->rhsLen) return true;

    // If suffix is exactly EPS, treat as nullable
    if (r->rhsLen == 1 && r->rhs[0] == SYMBOL_EPS_ID) return true;

    bool nullable = true;

    for (int i = pos; i < r->rhsLen; i++) {
        int X = r->rhs[i];

        // If grammar stores EPS inside longer RHS (shouldn't, but be safe)
        if (X == SYMBOL_EPS_ID) {
            // epsilon contributes nothing to lookahead besides nullability
            continue;
        }

        addSetExceptEps(outNoEps, &FIRST[X]);

        if (!setContains(&FIRST[X], SYMBOL_EPS_ID)) {
            nullable = false;
            break;
        }
    }

    return nullable;
}

// lifecycle

void initFirstFollowArrays(const Grammar *G, BitSet *FIRST, BitSet *FOLLOW) {
    for (int i = 0; i < G->numSymbols; i++) {
        setInit(&FIRST[i], G->numSymbols);
        setInit(&FOLLOW[i], G->numSymbols);
    }
}

void freeFirstFollowArrays(const Grammar *G, BitSet *FIRST, BitSet *FOLLOW) {
    for (int i = 0; i < G->numSymbols; i++) {
        setFree(&FIRST[i]);
        setFree(&FOLLOW[i]);
    }
}

// FIRST 

void computeFIRST(const Grammar *G, BitSet *FIRST) {
    // init FIRST of terminals
    for (int i = 0; i < G->numSymbols; i++) {
        setClearAll(&FIRST[i]);
    }

    // Terminals: FIRST(t) = {t}
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind == SYM_TERMINAL) {
            setAdd(&FIRST[i], i);
        }
    }

    // Ensure EPS behaves: FIRST(EPS) = {EPS}
    setClearAll(&FIRST[SYMBOL_EPS_ID]);
    setAdd(&FIRST[SYMBOL_EPS_ID], SYMBOL_EPS_ID);

    bool changed;
    do {
        changed = false;

        for (int p = 0; p < G->numProds; p++) {
            const Production *P = &G->prods[p];
            int A = P->lhs;

            for (int a = 0; a < P->numAlts; a++) {
                const RHS *r = &P->alts[a];

                // epsilon production
                if (r->rhsLen == 1 && r->rhs[0] == SYMBOL_EPS_ID) {
                    changed |= setAdd(&FIRST[A], SYMBOL_EPS_ID);
                    continue;
                }

                bool allNullable = true;

                for (int i = 0; i < r->rhsLen; i++) {
                    int X = r->rhs[i];

                    if (X == SYMBOL_EPS_ID) {
                        // skip (shouldn't happen in long RHS)
                        continue;
                    }

                    // add FIRST(X) \ {EPS} into FIRST(A)
                    changed |= addSetExceptEps(&FIRST[A], &FIRST[X]);

                    if (!setContains(&FIRST[X], SYMBOL_EPS_ID)) {
                        allNullable = false;
                        break;
                    }
                }

                if (allNullable) {
                    changed |= setAdd(&FIRST[A], SYMBOL_EPS_ID);
                }
            }
        }
    } while (changed);
}

// FOLLOW 

void computeFOLLOW(const Grammar *G, const BitSet *FIRST, BitSet *FOLLOW) {
    // init FOLLOW empty
    for (int i = 0; i < G->numSymbols; i++) {
        setClearAll(&FOLLOW[i]);
    }

    // FOLLOW(start) includes $
    if (G->startSymbol >= 0) {
        setAdd(&FOLLOW[G->startSymbol], SYMBOL_DOLLAR_ID);
    }

    BitSet tmpNoEps;
    setInit(&tmpNoEps, G->numSymbols);

    bool changed;
    do {
        changed = false;

        for (int p = 0; p < G->numProds; p++) {
            const Production *P = &G->prods[p];
            int A = P->lhs;

            for (int a = 0; a < P->numAlts; a++) {
                const RHS *r = &P->alts[a];

                // ignore epsilon production
                if (r->rhsLen == 1 && r->rhs[0] == SYMBOL_EPS_ID) continue;

                for (int i = 0; i < r->rhsLen; i++) {
                    int B = r->rhs[i];

                    if (B == SYMBOL_EPS_ID) continue;
                    if (G->symbols[B].kind != SYM_NONTERMINAL) continue;

                    // tmpNoEps = FIRST(beta) \ {EPS}, where beta = rhs[i+1..]
                    setClearAll(&tmpNoEps);
                    bool betaNullable = firstOfSequence_noeps(FIRST, r, i + 1, &tmpNoEps);

                    // FOLLOW(B) += tmpNoEps
                    changed |= setUnionInto(&FOLLOW[B], &tmpNoEps);

                    // FOLLOW(B) += FOLLOW(A) if beta nullable (or beta empty)
                    if (betaNullable) {
                        // Ensure EPS never enters FOLLOW
                        bool beforeEps = setContains(&FOLLOW[B], SYMBOL_EPS_ID);
                        changed |= setUnionInto(&FOLLOW[B], &FOLLOW[A]);
                        // remove EPS if it slipped in from bad data (shouldn’t)
                        if (!beforeEps) {
                            // even if it was present earlier, FOLLOW should not have EPS
                            setRemove(&FOLLOW[B], SYMBOL_EPS_ID);
                        } else {
                            setRemove(&FOLLOW[B], SYMBOL_EPS_ID);
                        }
                    }

                    // FOLLOW should never contain EPS
                    setRemove(&FOLLOW[B], SYMBOL_EPS_ID);
                }
            }
        }
    } while (changed);

    setFree(&tmpNoEps);
}

// printing 

static void printSetByNames(const Grammar *G, const BitSet *s, FILE *out) {
    fprintf(out, "{ ");
    for (int x = setNext(s, 0); x != -1; x = setNext(s, x + 1)) {
        fprintf(out, "%s ", G->symbols[x].name);
    }
    fprintf(out, "}");
}

void printFIRST(const Grammar *G, const BitSet *FIRST, FILE *out) {
    fprintf(out, "\n===== FIRST sets =====\n");
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind != SYM_NONTERMINAL) continue;
        fprintf(out, "FIRST(%s) = ", G->symbols[i].name);
        printSetByNames(G, &FIRST[i], out);
        fprintf(out, "\n");
    }
}

void printFOLLOW(const Grammar *G, const BitSet *FOLLOW, FILE *out) {
    fprintf(out, "\n===== FOLLOW sets =====\n");
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind != SYM_NONTERMINAL) continue;
        fprintf(out, "FOLLOW(%s) = ", G->symbols[i].name);
        printSetByNames(G, &FOLLOW[i], out);
        fprintf(out, "\n");
    }
}