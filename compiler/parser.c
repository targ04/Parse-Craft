/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


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

static bool isEmptyEntry(PTEntry e) { return e.prodIdx ==-1; }

static PTEntry synchEntry(void) {
    PTEntry e; e.prodIdx = -2; e.altIdx = -2;
    return e;
}
static bool isSynchEntry(PTEntry e) { return e.prodIdx == -2; }


static void reportSyntaxError(FILE *out, int line, const char *msg,
                              const char *got, const char *expected) {
    fprintf(out, "[Syntax Error] line %d: %s. got=%s expected=%s\n",
            line, msg, got ? got : "?", expected ? expected : "?");
}

/* parse tree internals */
static tokenInfo getNextNonCommentToken(twinBuffer *B) {
    tokenInfo tk = getNextToken(B);
    while (tk.type == TK_COMMENT) {
        tk = getNextToken(B);
    }
    return tk;
}
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
// static void printTreePretty(
//     const Grammar *G,
//     const ParseTreeNode *node,
//     FILE *out,
//     int depth,
//     bool isLastChild
// ) {
//     if (!node) return;

//     // Indentation
//     for (int i = 0; i < depth; i++) {
//         fprintf(out, "│   ");
//     }

//     if (depth > 0) {
//         fprintf(out, isLastChild ? "└── " : "├── ");
//     }

//     const char *name = G->symbols[node->symbolId].name;

//     if (node->firstChild == NULL) {
//         // Leaf
//         if (node->hasToken) {
//             fprintf(out, "%s  (lexeme: %s, line: %d)\n",
//                     name,
//                     node->tk.lexeme,
//                     node->tk.lineNo);
//         } else {
//             fprintf(out, "%s\n", name);
//         }
//     } else {
//         fprintf(out, "%s\n", name);
//     }

//     // Count children
//     const ParseTreeNode *child = node->firstChild;
//     int count = 0;
//     while (child) {
//         count++;
//         child = child->nextSibling;
//     }

//     // Recurse on children
//     child = node->firstChild;
//     int idx = 0;
//     while (child) {
//         printTreePretty(G, child, out, depth + 1, (idx == count - 1));
//         child = child->nextSibling;
//         idx++;
//     }
// }

void printParseTree(const Grammar *G, const ParseTreeNode *root, FILE *out) {

    if (!root) return;

    //fprintf(out, "\n------------------------ PARSE TREE (STRUCTURE) ------------------------\n\n");

    //printTreePretty(G, root, out, 0, true);
    //fprintf("\n\n");
    fprintf(out, "------------------------ PARSE TREE (TABLE FORMAT) ------------------------\n\n");

    printPTHeader(out);        
    inorderNary(G, root, out); 
}
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


// helper: identify “panic sync boundary” terminals by grammar symbol name
static bool isBoundaryTerminalSym(const Grammar *G, int termSymId) {
    const char *nm = G->symbols[termSymId].name;
    return strcmp(nm, "TK_SEM") == 0 ||
           strcmp(nm, "TK_END") == 0 ||
           strcmp(nm, "TK_ENDIF") == 0 ||
           strcmp(nm, "TK_ENDWHILE") == 0 ||
           strcmp(nm, "TK_ENDRECORD") == 0 ||
           strcmp(nm, "TK_ENDUNION") == 0 ||
           strcmp(nm, "TK_ELSE") == 0 ||
           strcmp(nm, "TK_CL") == 0 ||
           strcmp(nm, "TK_SQR") == 0;
}

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

    // PASS 1: normal LL(1) fills
    for (int pi = 0; pi < G->numProds; pi++) {
        const Production *P = &G->prods[pi];
        int A = P->lhs;
        int row = T->rowOfSymbolId[A];
        if (row < 0) continue;

        for (int ai = 0; ai < P->numAlts; ai++) {
            const RHS *rhs = &P->alts[ai];

            setClearAll(&tmp);
            bool nullable = firstOfSequence_noeps(FIRST, rhs, 0, &tmp);

            // FIRST(alpha) \ {EPS}
            for (int t = setNext(&tmp, 0); t != -1; t = setNext(&tmp, t + 1)) {
                if (t == SYMBOL_EPS_ID) continue;

                int col = T->colOfSymbolId[t];
                if (col < 0) continue;

                int k = idx2D(T, row, col);
                PTEntry cur = T->cell[k];
                PTEntry neu = (PTEntry){pi, ai};

                if (isEmptyEntry(cur)) {
                    T->cell[k] = neu;
                    st.numFilled++;
                } else if (!isSynchEntry(cur) &&
                           (cur.prodIdx != neu.prodIdx || cur.altIdx != neu.altIdx)) {
                    st.numConflicts++;
                    if (conflictOut) {
                        fprintf(conflictOut,
                            "[LL(1) CONFLICT] M[%s, %s] already has (%d,%d), tried (%d,%d)\n",
                            G->symbols[A].name, G->symbols[t].name,
                            cur.prodIdx, cur.altIdx, neu.prodIdx, neu.altIdx);
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
                    } else if (!isSynchEntry(cur) &&
                               (cur.prodIdx != neu.prodIdx || cur.altIdx != neu.altIdx)) {
                        st.numConflicts++;
                        if (conflictOut) {
                            fprintf(conflictOut,
                                "[LL(1) CONFLICT] M[%s, %s] already has (%d,%d), tried (%d,%d)\n",
                                G->symbols[A].name, G->symbols[b].name,
                                cur.prodIdx, cur.altIdx, neu.prodIdx, neu.altIdx);
                        }
                    }
                }
            }
        }
    }

    // PASS 2: add SYNCH entries (classic)
    // FOLLOW(A) cells that are still empty
    for (int r = 0; r < T->nRows; r++) {
        int A = T->ntSymbolIdByRow[r];

        for (int b = setNext(&FOLLOW[A], 0); b != -1; b = setNext(&FOLLOW[A], b + 1)) {
            if (b == SYMBOL_EPS_ID) continue;

            //Only boundary tokens becomen SYNCH
            //if (!isBoundaryTerminalSym(G, b)) continue;
            int c = T->colOfSymbolId[b];
            if (c < 0) continue;

            int k = idx2D(T, r, c);
            if (isEmptyEntry(T->cell[k])) {
                T->cell[k] = synchEntry();
                st.numFilled++;
            }
        }
    }

    // PASS 3: global boundary-token SYNCH (repo style)
    // For ANY nonterminal row A, if M[A, boundaryTok] is empty => SYNCH.
    // This is what fixes cases like <otherStmts> with lookahead TK_END.
    for (int r = 0; r < T->nRows; r++) {
        for (int c = 0; c < T->nCols; c++) {
            int termSym = T->tSymbolIdByCol[c];
            if (!isBoundaryTerminalSym(G, termSym)) continue;

            int k = idx2D(T, r, c);
            if (isEmptyEntry(T->cell[k])) {
                T->cell[k] = synchEntry();
                st.numFilled++;
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

            if (isSynchEntry(e)) {
                fprintf(out, "M[%s, %s] = SYNCH\n",
                        G->symbols[A].name, G->symbols[t].name);
                continue;
            }

            const Production *P = &G->prods[e.prodIdx];
            const RHS *rhs = &P->alts[e.altIdx];

            fprintf(out, "M[%s, %s] = %s ===> ",
                    G->symbols[A].name, G->symbols[t].name, G->symbols[P->lhs].name);
            for (int i = 0; i < rhs->rhsLen; i++) {
                fprintf(out, "%s%s", G->symbols[rhs->rhs[i]].name,
                        (i + 1 < rhs->rhsLen) ? " " : "");
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
// “panic boundary tokens”: don't discard these; pop stack until recover
static bool isPanicBoundary(TokenType t) {
    return t == TK_SEM || t == TK_ELSE || t == TK_ENDIF || t == TK_ENDWHILE ||
           t == TK_END || t == TK_ENDRECORD || t == TK_ENDUNION ||
           t == TK_SQR || t == TK_CL || t == TK_THEN || t == TK_EOF;
}

static bool isValueTerminalSym(const Grammar *G, int symId) {
    const char *nm = G->symbols[symId].name;
    return (strcmp(nm, "TK_ID") == 0) ||
           (strcmp(nm, "TK_NUM") == 0) ||
           (strcmp(nm, "TK_RNUM") == 0) ||
           (strcmp(nm, "TK_FIELDID") == 0) ||
           (strcmp(nm, "TK_RUID") == 0) ||
           (strcmp(nm, "TK_FUNID") == 0);
}

static bool isStartOfConstructTok(TokenType t) {
    return t == TK_TYPE || t == TK_IF || t == TK_WHILE || t == TK_READ || t == TK_WRITE ||
           t == TK_CALL || t == TK_SQL || t == TK_RETURN || t == TK_FUNID || t == TK_MAIN ||
           t == TK_RECORD || t == TK_UNION || t == TK_DEFINETYPE;
}

static bool isDelimiterTok(TokenType t) {
    return t == TK_SEM || t == TK_COMMA || t == TK_SQR || t == TK_CL;
}

// Can we consult parse table for (A, lookSym)?
static bool tableHasAction(const ParseTable *T, int A, int lookSym) {
    int r = T->rowOfSymbolId[A];
    int c = T->colOfSymbolId[lookSym];
    if (r < 0 || c < 0) return false;
    PTEntry e = T->cell[idx2D(T, r, c)];
    return !isEmptyEntry(e); // SYNCH counts as action too (non-empty)
}
static bool isExpressionNonTerminal(const Grammar *G, int symId) {
    const char *nm = G->symbols[symId].name;
    return strcmp(nm, "<arithmeticExpression>") == 0 ||
           strcmp(nm, "<term>") == 0 ||
           strcmp(nm, "<termPrime>") == 0 ||
           strcmp(nm, "<expPrime>") == 0 ||
           strcmp(nm, "<factor>") == 0 ||
           strcmp(nm, "<var>") == 0 ||
           strcmp(nm, "<singleOrRecId>") == 0 ||
           strcmp(nm, "<optionSingleConstructed>") == 0 ||
           strcmp(nm, "<moreExpansions>") == 0;
}

// the function 
ParseTreeNode* parseInputSourceCode(
    twinBuffer *B,
    const Grammar *G,
    const ParseTable *T,
    const BitSet *FOLLOW,
    FILE *out,
    bool *okOut
) {
    (void)FOLLOW; // avoid unused warning
    bool ok = true;

    ParseTreeNode *root = newNode(G->startSymbol, false);

    PTStack st;
    sInit(&st);

    ParseTreeNode *dollarNode = newNode(SYMBOL_DOLLAR_ID, true);
    sPush(&st, (StackItem){SYMBOL_DOLLAR_ID, dollarNode});
    sPush(&st, (StackItem){G->startSymbol, root});

    tokenInfo look = getNextNonCommentToken(B);
    int lookSym = terminalSymbolIdFromToken(G, look);

    int lastMatchedLine = 0;

    while (!sEmpty(&st)) {
        StackItem top = sPeek(&st);
        int X = top.symId;
        ParseTreeNode *Xnode = top.node;

        if (X == SYMBOL_EPS_ID) { sPop(&st); continue; }

        // unmapped lexer token
        if (lookSym < 0) {
            ok = false;
            reportSyntaxError(out, look.lineNo, "Unmapped token from lexer",
                              tokenToString(look.type), "known terminal");
            look = getNextNonCommentToken(B);
            lookSym = terminalSymbolIdFromToken(G, look);
            continue;
        }

        // TERMINAL 
        if (X == SYMBOL_DOLLAR_ID || G->symbols[X].kind == SYM_TERMINAL) {
            if (X == lookSym) {
                if (X != SYMBOL_DOLLAR_ID) {
                    Xnode->tk = look;
                    Xnode->hasToken = true;
                    lastMatchedLine = look.lineNo;
                }
                sPop(&st);
                look = getNextNonCommentToken(B);
                lookSym = terminalSymbolIdFromToken(G, look);
                continue;
            }

            ok = false;

            const bool valueExpected = isValueTerminalSym(G, X);
            const bool boundary = (look.type == TK_EOF) || isPanicBoundary(look.type);
            const bool startOfNew = isStartOfConstructTok(look.type);
            const int reportLine = (lastMatchedLine > 0 ? lastMatchedLine : look.lineNo);

            // if boundary token, prefer INSERT missing terminal (pop X)
            if (boundary) {
                reportSyntaxError(out, reportLine,
                                  "Terminal mismatch (inserting missing token)",
                                  tokenToString(look.type), G->symbols[X].name);
                sPop(&st);
                continue;
            }

            if (valueExpected) {
                if (startOfNew || isDelimiterTok(look.type)) {
                    reportSyntaxError(out, reportLine,
                                      "Terminal mismatch (inserting missing token)",
                                      tokenToString(look.type), G->symbols[X].name);
                    sPop(&st);
                } else {
                    reportSyntaxError(out, look.lineNo,
                                      "Terminal mismatch (discarding unexpected token)",
                                      tokenToString(look.type), G->symbols[X].name);
                    look = getNextNonCommentToken(B);
                    lookSym = terminalSymbolIdFromToken(G, look);
                }
            } else {
                reportSyntaxError(out, reportLine,
                                  "Terminal mismatch (inserting missing token)",
                                  tokenToString(look.type), G->symbols[X].name);
                sPop(&st);
            }
            continue;
        }

        //  NONTERMINAL 
        int row = T->rowOfSymbolId[X];
        int col = T->colOfSymbolId[lookSym];
        if (row < 0 || col < 0) {
            ok = false;
            reportSyntaxError(out, look.lineNo, "Parse table index error",
                              tokenToString(look.type), G->symbols[X].name);
            look = getNextNonCommentToken(B);
            lookSym = terminalSymbolIdFromToken(G, look);
            continue;
        }

        PTEntry e = T->cell[idx2D(T, row, col)];
        
        // SYNCH cell: pop nonterminal, don't consume input
        if (isSynchEntry(e)) {
            ok = false;
            reportSyntaxError(out, look.lineNo,
                              "Invalid token (synch): popping nonterminal",
                              tokenToString(look.type), G->symbols[X].name);
            sPop(&st);
            continue;
        }

        // normal production
        if (!isEmptyEntry(e)) {
            sPop(&st);

            const Production *P = &G->prods[e.prodIdx];
            const RHS *rhs = &P->alts[e.altIdx];

            if (rhs->rhsLen == 1 && rhs->rhs[0] == SYMBOL_EPS_ID) {
                ParseTreeNode *eps = newNode(SYMBOL_EPS_ID, true);
                addChild(Xnode, eps);
                continue;
            }

            ParseTreeNode **child =
                (ParseTreeNode**)xmalloc(sizeof(ParseTreeNode*) * (size_t)rhs->rhsLen);

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
            continue;
        }

        // ERROR CELL: panic mode 
        ok = false;

        if (isPanicBoundary(look.type)) {
            // Keep boundary token; pop stack until it can be handled.
            while (!sEmpty(&st)) {
                StackItem ttop = sPeek(&st);
                int S = ttop.symId;

                // 1) If top is terminal:
                if (S == SYMBOL_DOLLAR_ID || G->symbols[S].kind == SYM_TERMINAL) {
                    // If terminal matches boundary, stop popping (parser can proceed normally)
                    if (S == lookSym) break;

                    // Otherwise: treat as missing terminal insertion (classic strategy)
                    const int reportLine = (lastMatchedLine > 0 ? lastMatchedLine : look.lineNo);
                    reportSyntaxError(out, reportLine,
                                    "Terminal mismatch (inserting missing token)",
                                    tokenToString(look.type), G->symbols[S].name);
                    sPop(&st);
                    continue;
                }

                // 2) If top is nonterminal and it has an action for lookahead, stop.
                if (tableHasAction(T, S, lookSym)) break;
                if (isExpressionNonTerminal(G, S)) {
                    sPop(&st);
                    continue;
                }
                // 3) Otherwise pop nonterminal (sync)
                reportSyntaxError(out, look.lineNo,
                                "Sync (panic): popping nonterminal",
                                tokenToString(look.type), G->symbols[S].name);
                sPop(&st);
            }

            // If stack empty but not EOF, consume to avoid infinite loop
            if (sEmpty(&st) && look.type != TK_EOF) {
                look = getNextNonCommentToken(B);
                lookSym = terminalSymbolIdFromToken(G, look);
            }
            continue;
        }

        // normal discard strategy (non-boundary token)
        reportSyntaxError(out, look.lineNo, "Error cell: discarding token",
                          tokenToString(look.type), G->symbols[X].name);
        look = getNextNonCommentToken(B);
        lookSym = terminalSymbolIdFromToken(G, look);
    }

    sFree(&st);
    if (okOut) *okOut = ok;
    return root;
}