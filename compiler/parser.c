#include "parser.h"
#include <stdlib.h>
#include <string.h>

/* small utils */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

static inline int idx2D(const ParseTable *T, int r, int c) {
    return r * T->nCols + c;
}

static PTEntry emptyEntry(void) {
    PTEntry e; e.prodIdx = -1; e.altIdx = -1; return e;
}

static bool isEmptyEntry(PTEntry e) { return e.prodIdx < 0; }

static void reportSyntaxError(FILE *out, int line, const char *msg,
                              const char *got, const char *expected) {
    fprintf(out, "[Syntax Error] line %d: %s. got=%s expected=%s\n",
            line, msg, got ? got : "?", expected ? expected : "?");
}

/* parse tree internals */

static ParseTreeNode* newNode(int symbolId, bool isTerminal) {
    ParseTreeNode *n = (ParseTreeNode*)xmalloc(sizeof(ParseTreeNode));
    n->symbolId = symbolId;
    n->isTerminal = isTerminal;
    n->hasToken = false;
    n->parent = NULL;
    n->firstChild = NULL;
    n->nextSibling = NULL;
    memset(&n->tk, 0, sizeof(n->tk));
    return n;
}

static void addChild(ParseTreeNode *parent, ParseTreeNode *child) {
    child->parent = parent;
    if (!parent->firstChild) {
        parent->firstChild = child;
    } else {
        ParseTreeNode *cur = parent->firstChild;
        while (cur->nextSibling) cur = cur->nextSibling;
        cur->nextSibling = child;
    }
}

void freeParseTree(ParseTreeNode *root) {
    if (!root) return;
    ParseTreeNode *ch = root->firstChild;
    while (ch) {
        ParseTreeNode *nx = ch->nextSibling;
        freeParseTree(ch);
        ch = nx;
    }
    free(root);
}


static void printPTHeader(FILE *out) {
    fprintf(out, "%-20s %-8s %-18s %-15s %-22s %-8s %-22s\n",
            "lexeme", "lineno", "tokenName", "valueIfNumber",
            "parentNodeSymbol", "isLeaf", "NodeSymbol");
    fprintf(out, "----------------------------------------------------------------------------------------------\n");
}

static void formatValueIfNumber(const ParseTreeNode *n, char *buf, size_t cap) {
    // default
    snprintf(buf, cap, "----");
    if (!n->hasToken) return;

    if (n->tk.type == TK_NUM) {
        // integer value
        snprintf(buf, cap, "%lld", (long long)n->tk.value.intVal);
    } else if (n->tk.type == TK_RNUM) {
        // real value
        snprintf(buf, cap, "%.6f", n->tk.value.realVal);
    }
}

static const char *parentSymbolName(const Grammar *G, const ParseTreeNode *n) {
    if (!n || !n->parent) return "ROOT";
    return G->symbols[n->parent->symbolId].name;
}

static void printOneNodeLine(const Grammar *G, const ParseTreeNode *n, FILE *out) {
    const bool isLeaf = (n->firstChild == NULL);
    const char *nodeSym = G->symbols[n->symbolId].name;
    const char *parentSym = parentSymbolName(G, n);

    const char *lexeme = "----";
    const char *tokenName = "----";
    int lineno = -1;
    char valbuf[64];
    snprintf(valbuf, sizeof(valbuf), "----");

    if (isLeaf) {
        // For leaves, print lexeme/tokenName/line/value if available
        tokenName = nodeSym; // terminal symbol name in grammar is like "TK_ID", "TK_READ", etc.

        // EPS leaf: lexeme must be "----" as per convention
        if (n->symbolId != SYMBOL_EPS_ID) {
            if (n->hasToken) {
                lexeme = n->tk.lexeme;
                lineno = n->tk.lineNo;
                formatValueIfNumber(n, valbuf, sizeof(valbuf));
            } else {
                // leaf but no token attached (can happen for inserted/missing terminals in recovery)
                // Keep lexeme/value as ----; line unknown
                lineno = -1;
            }
        } else {
            // EPS
            lineno = -1;
        }
    }

    // Spec wants lineno column always present; for non-leaf or unknown line print -1 (or 0).
    fprintf(out, "%-20s %-8d %-18s %-15s %-22s %-8s %-22s\n",
            lexeme,
            lineno,
            tokenName,
            valbuf,
            parentSym,
            isLeaf ? "yes" : "no",
            nodeSym);
}

// inorder for n-ary: leftmost child -> node -> remaining children
static void inorderNary(const Grammar *G, const ParseTreeNode *n, FILE *out) {
    if (!n) return;

    const ParseTreeNode *c = n->firstChild;

    if (!c) {
        // leaf
        printOneNodeLine(G, n, out);
        return;
    }

    // visit leftmost child
    inorderNary(G, c, out);

    // visit node
    printOneNodeLine(G, n, out);

    // visit remaining children
    c = c->nextSibling;
    while (c) {
        inorderNary(G, c, out);
        c = c->nextSibling;
    }
}

void printParseTree(const Grammar *G, const ParseTreeNode *root, FILE *out) {
    if (!out) return;
    printPTHeader(out);
    inorderNary(G, root, out);
}

/*  FIRST(seq) helper  */
// FIRST(sequence) \ {EPS} into outNoEps; returns true if sequence nullable
// static bool firstOfSequence_noeps(
//     const BitSet *FIRST,
//     const RHS *r,
//     int pos,
//     BitSet *outNoEps
// ) {
//     if (pos >= r->rhsLen) return true;
//     if (r->rhsLen == 1 && r->rhs[0] == SYMBOL_EPS_ID) return true;

//     bool nullable = true;

//     for (int i = pos; i < r->rhsLen; i++) {
//         int X = r->rhs[i];
//         if (X == SYMBOL_EPS_ID) continue;

//         for (int t = setNext(&FIRST[X], 0); t != -1; t = setNext(&FIRST[X], t + 1)) {
//             if (t == SYMBOL_EPS_ID) continue;
//             setAdd(outNoEps, t);
//         }

//         if (!setContains(&FIRST[X], SYMBOL_EPS_ID)) {
//             nullable = false;
//             break;
//         }
//     }
//     return nullable;
// }

/* ParseTable lifecycle */

void initParseTable(const Grammar *G, ParseTable *T) {
    memset(T, 0, sizeof(*T));

    T->rowOfSymbolId = (int*)xmalloc(sizeof(int) * (size_t)G->numSymbols);
    T->colOfSymbolId = (int*)xmalloc(sizeof(int) * (size_t)G->numSymbols);
    for (int i = 0; i < G->numSymbols; i++) {
        T->rowOfSymbolId[i] = -1;
        T->colOfSymbolId[i] = -1;
    }

    // rows: nonterminals
    T->nRows = 0;
    for (int i = 0; i < G->numSymbols; i++)
        if (G->symbols[i].kind == SYM_NONTERMINAL) T->nRows++;

    T->ntSymbolIdByRow = (int*)xmalloc(sizeof(int) * (size_t)T->nRows);
    int r = 0;
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind == SYM_NONTERMINAL) {
            T->ntSymbolIdByRow[r] = i;
            T->rowOfSymbolId[i] = r;
            r++;
        }
    }

    // cols: terminals excluding EPS
    T->nCols = 0;
    for (int i = 0; i < G->numSymbols; i++)
        if (G->symbols[i].kind == SYM_TERMINAL && i != SYMBOL_EPS_ID) T->nCols++;

    T->tSymbolIdByCol = (int*)xmalloc(sizeof(int) * (size_t)T->nCols);
    int c = 0;
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind == SYM_TERMINAL && i != SYMBOL_EPS_ID) {
            T->tSymbolIdByCol[c] = i;
            T->colOfSymbolId[i] = c;
            c++;
        }
    }

    // cells
    T->cell = (PTEntry*)xmalloc(sizeof(PTEntry) * (size_t)(T->nRows * T->nCols));
    for (int i = 0; i < T->nRows * T->nCols; i++) T->cell[i] = emptyEntry();
}

void freeParseTable(ParseTable *T) {
    free(T->ntSymbolIdByRow);
    free(T->tSymbolIdByCol);
    free(T->rowOfSymbolId);
    free(T->colOfSymbolId);
    free(T->cell);
    memset(T, 0, sizeof(*T));
}

/* createParseTable */

ParseTableStats createParseTable(
    const Grammar *G,
    const BitSet *FIRST,
    const BitSet *FOLLOW,
    ParseTable *T,
    FILE *conflictOut
) {
    ParseTableStats st = {0, 0};

    BitSet tmp;
    setInit(&tmp, G->numSymbols);

    for (int pi = 0; pi < G->numProds; pi++) {
        const Production *P = &G->prods[pi];
        int A = P->lhs;
        int row = T->rowOfSymbolId[A];
        if (row < 0) continue;

        for (int ai = 0; ai < P->numAlts; ai++) {
            const RHS *rhs = &P->alts[ai];

            setClearAll(&tmp);
            bool nullable = firstOfSequence_noeps(FIRST, rhs, 0, &tmp);

            // FIRST(alpha)\{EPS}
            for (int t = setNext(&tmp, 0); t != -1; t = setNext(&tmp, t + 1)) {
                int col = T->colOfSymbolId[t];
                if (col < 0) continue;

                int k = idx2D(T, row, col);
                PTEntry cur = T->cell[k];
                PTEntry neu = (PTEntry){pi, ai};

                if (isEmptyEntry(cur)) {
                    T->cell[k] = neu;
                    st.numFilled++;
                } else if (cur.prodIdx != neu.prodIdx || cur.altIdx != neu.altIdx) {
                    st.numConflicts++;
                    if (conflictOut) {
                        fprintf(conflictOut,
                            "[LL(1) CONFLICT] M[%s, %s] already has alt %d, tried alt %d\n",
                            G->symbols[A].name, G->symbols[t].name, cur.altIdx, neu.altIdx);
                    }
                }
            }

            // if nullable => FOLLOW(A)
            if (nullable) {
                for (int b = setNext(&FOLLOW[A], 0); b != -1; b = setNext(&FOLLOW[A], b + 1)) {
                    if (b == SYMBOL_EPS_ID) continue;
                    int col = T->colOfSymbolId[b];
                    if (col < 0) continue;

                    int k = idx2D(T, row, col);
                    PTEntry cur = T->cell[k];
                    PTEntry neu = (PTEntry){pi, ai};

                    if (isEmptyEntry(cur)) {
                        T->cell[k] = neu;
                        st.numFilled++;
                    } else if (cur.prodIdx != neu.prodIdx || cur.altIdx != neu.altIdx) {
                        st.numConflicts++;
                        if (conflictOut) {
                            fprintf(conflictOut,
                                "[LL(1) CONFLICT] M[%s, %s] already has alt %d, tried alt %d\n",
                                G->symbols[A].name, G->symbols[b].name, cur.altIdx, neu.altIdx);
                        }
                    }
                }
            }
        }
    }

    setFree(&tmp);
    return st;
}

void printParseTable(const Grammar *G, const ParseTable *T, FILE *out) {
    fprintf(out, "\n===== PARSE TABLE (sparse) =====\n");
    for (int r = 0; r < T->nRows; r++) {
        int A = T->ntSymbolIdByRow[r];
        for (int c = 0; c < T->nCols; c++) {
            PTEntry e = T->cell[idx2D(T, r, c)];
            if (isEmptyEntry(e)) continue;

            int t = T->tSymbolIdByCol[c];
            const Production *P = &G->prods[e.prodIdx];
            const RHS *rhs = &P->alts[e.altIdx];

            fprintf(out, "M[%s, %s] = %s ===> ",
                    G->symbols[A].name, G->symbols[t].name, G->symbols[P->lhs].name);
            for (int i = 0; i < rhs->rhsLen; i++) {
                fprintf(out, "%s%s", G->symbols[rhs->rhs[i]].name, (i + 1 < rhs->rhsLen) ? " " : "");
            }
            fprintf(out, "\n");
        }
    }
}

/* token mapping */

int terminalSymbolIdFromToken(const Grammar *G, tokenInfo tk) {
    // Your lexer already returns tokenToString(TokenType) like "TK_ID", ...
    // Also ensure tokenToString(TK_EOF) returns "$" OR we map it manually.
    const char *name = tokenToString(tk.type);

    // If your tokenToString prints "TK_EOF", but grammar uses "$", do this:
    if (tk.type == TK_EOF) name = "$";

    for (int i = 0; i < G->numSymbols; i++) {
        if (strcmp(G->symbols[i].name, name) == 0) return i;
    }
    return -1;
}

/* Predictive parse with parse tree  */

typedef struct {
    int symId;
    ParseTreeNode *node;
} StackItem;

typedef struct {
    StackItem *a;
    int top;
    int cap;
} PTStack;

static void sInit(PTStack *s) {
    s->cap = 256;
    s->top = -1;
    s->a = (StackItem*)xmalloc(sizeof(StackItem) * (size_t)s->cap);
}
static void sFree(PTStack *s) { free(s->a); }
static bool sEmpty(const PTStack *s) { return s->top < 0; }
static void sPush(PTStack *s, StackItem it) {
    if (s->top + 1 >= s->cap) {
        s->cap *= 2;
        s->a = (StackItem*)realloc(s->a, sizeof(StackItem) * (size_t)s->cap);
        if (!s->a) { fprintf(stderr, "Out of memory\n"); exit(1); }
    }
    s->a[++s->top] = it;
}
static StackItem sPop(PTStack *s) { return s->a[s->top--]; }
static StackItem sPeek(const PTStack *s) { return s->a[s->top]; }

ParseTreeNode* parseInputSourceCode(
    twinBuffer *B,
    const Grammar *G,
    const ParseTable *T,
    const BitSet *FOLLOW,
    FILE *out,
    bool *okOut
) {
    bool ok = true;

    // build parse tree root: start symbol
    ParseTreeNode *root = newNode(G->startSymbol, false);

    PTStack st;
    sInit(&st);

    // push $ and start
    ParseTreeNode *dollarNode = newNode(SYMBOL_DOLLAR_ID, true);
    // optional: attach $ as sibling/child? Usually not in parse tree. Skip attaching.
    sPush(&st, (StackItem){SYMBOL_DOLLAR_ID, dollarNode});
    sPush(&st, (StackItem){G->startSymbol, root});

    tokenInfo look = getNextToken(B);
    int lookSym = terminalSymbolIdFromToken(G, look);

    while (!sEmpty(&st)) {
        StackItem top = sPeek(&st);
        int X = top.symId;
        ParseTreeNode *Xnode = top.node;

        // Skip EPS on stack (rare: we usually don't push EPS)
        if (X == SYMBOL_EPS_ID) {
            sPop(&st);
            continue;
        }

        // If lexer token couldn't be mapped, advance
        if (lookSym < 0) {
            ok = false;
            reportSyntaxError(out, look.lineNo, "Unmapped token from lexer",
                              tokenToString(look.type), "known terminal");
            look = getNextToken(B);
            lookSym = terminalSymbolIdFromToken(G, look);
            continue;
        }

        // Terminal (including $)
        if (G->symbols[X].kind == SYM_TERMINAL || X == SYMBOL_DOLLAR_ID) {
            if (X == lookSym) {
                // match: annotate node with token
                if (X != SYMBOL_DOLLAR_ID) {
                    Xnode->tk = look;
                    Xnode->hasToken = true;
                }
                sPop(&st);

                look = getNextToken(B);
                lookSym = terminalSymbolIdFromToken(G, look);
            } else {
                // terminal mismatch: "insert" X (pop) to recover
                ok = false;
                reportSyntaxError(out, look.lineNo,
                                  "Terminal mismatch (inserting missing token)",
                                  tokenToString(look.type), G->symbols[X].name);
                sPop(&st);
                // do NOT advance look; we assumed missing terminal
            }
            continue;
        }

        // Nonterminal: consult table
        int row = T->rowOfSymbolId[X];
        int col = T->colOfSymbolId[lookSym];

        if (row < 0 || col < 0) {
            ok = false;
            reportSyntaxError(out, look.lineNo, "Parse table index error",
                              tokenToString(look.type), G->symbols[X].name);
            // discard token
            look = getNextToken(B);
            lookSym = terminalSymbolIdFromToken(G, look);
            continue;
        }

        PTEntry e = T->cell[idx2D(T, row, col)];

        if (!isEmptyEntry(e)) {
            // Apply production: pop nonterminal X
            sPop(&st);

            const Production *P = &G->prods[e.prodIdx];
            const RHS *rhs = &P->alts[e.altIdx];

            // Create children nodes in left-to-right order
            // Then push onto stack in reverse order with their node pointers.
            // If RHS is EPS: add EPS child (optional but nice for printing).
            if (rhs->rhsLen == 1 && rhs->rhs[0] == SYMBOL_EPS_ID) {
                ParseTreeNode *eps = newNode(SYMBOL_EPS_ID, true);
                addChild(Xnode, eps);
                // do not push EPS
                continue;
            }

            // Build an array of child pointers for pushing reverse
            ParseTreeNode **child = (ParseTreeNode**)xmalloc(sizeof(ParseTreeNode*) * (size_t)rhs->rhsLen);

            for (int i = 0; i < rhs->rhsLen; i++) {
                int sym = rhs->rhs[i];
                bool isT = (sym == SYMBOL_EPS_ID) ? true : (G->symbols[sym].kind == SYM_TERMINAL);
                ParseTreeNode *cn = newNode(sym, isT);
                addChild(Xnode, cn);
                child[i] = cn;
            }

            for (int i = rhs->rhsLen - 1; i >= 0; i--) {
                int sym = rhs->rhs[i];
                if (sym == SYMBOL_EPS_ID) continue;
                sPush(&st, (StackItem){sym, child[i]});
            }

            free(child);
        } else {
            // Error recovery:
            // If lookahead in FOLLOW(X) => pop X (sync), else discard token.
            ok = false;
            if (setContains(&FOLLOW[X], lookSym)) {
                reportSyntaxError(out, look.lineNo, "Sync: popping nonterminal",
                                  tokenToString(look.type), G->symbols[X].name);
                sPop(&st);
            } else {
                reportSyntaxError(out, look.lineNo, "Discarding token",
                                  tokenToString(look.type), G->symbols[X].name);
                look = getNextToken(B);
                lookSym = terminalSymbolIdFromToken(G, look);
            }
        }
    }

    sFree(&st);
    if (okOut) *okOut = ok;
    return root;
}