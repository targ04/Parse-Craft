#include "grammar.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// small utils 

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return q;
}

static void trimInPlace(char *s) {
    if (!s) return;
    // leading
    int i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);

    // trailing
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

// Splits by delimiter token (not regex). Returns pointer to next segment.
// Modifies string by inserting '\0'. Similar to strtok but for a multi-char delim.
static char *splitOnce(char *s, const char *delim) {
    char *p = strstr(s, delim);
    if (!p) return NULL;
    *p = '\0';
    return p + strlen(delim);
}

bool isNonTerminalName(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    return n >= 3 && s[0] == '<' && s[n - 1] == '>';
}

bool isEpsilonName(const char *s) {
    return s && strcmp(s, "EPS") == 0;
}

bool isDollarName(const char *s) {
    return s && strcmp(s, "$") == 0;
}

// dynamic arrays

static void ensureSymbolsCap(Grammar *G) {
    if (G->numSymbols < G->capSymbols) return;
    G->capSymbols = (G->capSymbols == 0) ? 64 : (2 * G->capSymbols);
    G->symbols = (Symbol*)xrealloc(G->symbols, sizeof(Symbol) * (size_t)G->capSymbols);
}

static void ensureProdsCap(Grammar *G) {
    if (G->numProds < G->capProds) return;
    G->capProds = (G->capProds == 0) ? 64 : (2 * G->capProds);
    G->prods = (Production*)xrealloc(G->prods, sizeof(Production) * (size_t)G->capProds);
}

static void ensureAltCap(Production *P, int *capAlts) {
    if (P->numAlts < *capAlts) return;
    *capAlts = (*capAlts == 0) ? 4 : (2 * (*capAlts));
    P->alts = (RHS*)xrealloc(P->alts, sizeof(RHS) * (size_t)(*capAlts));
}

// symbol table (simple linear search) 
// For CS F363 sized grammars, linear search is fine (few hundred symbols max).
// If we want, later we can swap this to hashmap.h/c without changing callers.

int findSymbolId(const Grammar *G, const char *name) {
    for (int i = 0; i < G->numSymbols; i++) {
        if (strcmp(G->symbols[i].name, name) == 0) return i;
    }
    return -1;
}

int getOrAddSymbol(Grammar *G, const char *name) {
    int id = findSymbolId(G, name);
    if (id != -1) return id;

    ensureSymbolsCap(G);
    id = G->numSymbols++;

    Symbol *S = &G->symbols[id];
    S->id = id;
    strncpy(S->name, name, MAX_SYMBOL_NAME - 1);
    S->name[MAX_SYMBOL_NAME - 1] = '\0';
    S->kind = isNonTerminalName(name) ? SYM_NONTERMINAL : SYM_TERMINAL;

    if (S->kind == SYM_TERMINAL) G->numTerminals++;
    else G->numNonTerminals++;

    return id;
}

bool isTerminalId(const Grammar *G, int symId) {
    (void)G;
    return symId >= 0 && G->symbols[symId].kind == SYM_TERMINAL;
}

bool isNonTerminalId(const Grammar *G, int symId) {
    (void)G;
    return symId >= 0 && G->symbols[symId].kind == SYM_NONTERMINAL;
}

// Grammar lifecycle

void initGrammar(Grammar *G) {
    memset(G, 0, sizeof(*G));
    G->startSymbol = -1;

    // Reserve fixed IDs for EPS and $
    // EPS
    ensureSymbolsCap(G);
    G->numSymbols = 0;
    G->numTerminals = 0;
    G->numNonTerminals = 0;

    (void)getOrAddSymbol(G, "EPS"); // id 0
    // Force EPS to be treated as terminal-ish placeholder (but we won’t put it into FIRST like a token)
    G->symbols[SYMBOL_EPS_ID].kind = SYM_TERMINAL;

    (void)getOrAddSymbol(G, "$");   // id 1
    G->symbols[SYMBOL_DOLLAR_ID].kind = SYM_TERMINAL;
}

void freeGrammar(Grammar *G) {
    if (!G) return;

    for (int i = 0; i < G->numProds; i++) {
        Production *P = &G->prods[i];
        for (int a = 0; a < P->numAlts; a++) {
            free(P->alts[a].rhs);
        }
        free(P->alts);
    }

    free(G->prods);
    free(G->symbols);

    memset(G, 0, sizeof(*G));
}

// Find existing Production by LHS, else create new.
static Production *getOrCreateProduction(Grammar *G, int lhsId) {
    for (int i = 0; i < G->numProds; i++) {
        if (G->prods[i].lhs == lhsId) return &G->prods[i];
    }

    ensureProdsCap(G);
    Production *P = &G->prods[G->numProds++];
    P->lhs = lhsId;
    P->alts = NULL;
    P->numAlts = 0;
    return P;
}

// Parse RHS alternative string like: "TK_MAIN <stmts> TK_END"
static RHS parseRHSAlt(Grammar *G, const char *altStr) {
    RHS r;
    r.rhs = NULL;
    r.rhsLen = 0;

    // copy to tokenize safely
    char buf[2048];
    strncpy(buf, altStr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trimInPlace(buf);

    if (buf[0] == '\0') {
        // empty alternative not expected; treat as EPS
        r.rhs = (int*)xmalloc(sizeof(int));
        r.rhs[0] = SYMBOL_EPS_ID;
        r.rhsLen = 1;
        return r;
    }

    // Special: "EPS"
    if (isEpsilonName(buf)) {
        r.rhs = (int*)xmalloc(sizeof(int));
        r.rhs[0] = SYMBOL_EPS_ID;
        r.rhsLen = 1;
        return r;
    }

    // Tokenize by whitespace
    int cap = 8;
    r.rhs = (int*)xmalloc(sizeof(int) * (size_t)cap);
    char *tok = strtok(buf, " \t\r\n");
    while (tok) {
        if (r.rhsLen >= cap) {
            cap *= 2;
            r.rhs = (int*)xrealloc(r.rhs, sizeof(int) * (size_t)cap);
        }
        int sid = getOrAddSymbol(G, tok);
        r.rhs[r.rhsLen++] = sid;

        tok = strtok(NULL, " \t\r\n");
    }

    return r;
}

int readGrammar(Grammar *G, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Could not open grammar file: %s\n", filename);
        return 0;
    }

    char line[4096];
    int lineNo = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineNo++;

        // strip newline
        line[strcspn(line, "\n")] = '\0';

        // ignore empty lines
        char tmp[4096];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        trimInPlace(tmp);
        if (tmp[0] == '\0') continue;

        // ignore comment lines if you ever add them
        if (tmp[0] == '%') continue;

        // split LHS and RHS by "===>"
        char work[4096];
        strncpy(work, tmp, sizeof(work) - 1);
        work[sizeof(work) - 1] = '\0';

        char *rhsPart = splitOnce(work, "===>");
        if (!rhsPart) {
            fprintf(stderr, "Grammar parse error at line %d: missing '===>'\n", lineNo);
            continue;
        }

        trimInPlace(work);     // LHS
        trimInPlace(rhsPart);  // RHS

        if (!isNonTerminalName(work)) {
            fprintf(stderr, "Grammar parse error at line %d: LHS must be nonterminal like <A>\n", lineNo);
            continue;
        }

        int lhsId = getOrAddSymbol(G, work);
        if (G->startSymbol == -1) G->startSymbol = lhsId;

        Production *P = getOrCreateProduction(G, lhsId);
        int capAlts = P->numAlts; // will grow via ensureAltCap

        // split RHS by '|'
        char rhsBuf[4096];
        strncpy(rhsBuf, rhsPart, sizeof(rhsBuf) - 1);
        rhsBuf[sizeof(rhsBuf) - 1] = '\0';

        char *cursor = rhsBuf;
        while (cursor) {
            char *next = strchr(cursor, '|');
            if (next) {
                *next = '\0';
                next++;
            }

            trimInPlace(cursor);
            if (cursor[0] == '\0') {
                // treat empty as EPS
                cursor = (char*)"EPS";
            }

            ensureAltCap(P, &capAlts);
            P->alts[P->numAlts++] = parseRHSAlt(G, cursor);

            cursor = next;
        }
    }

    fclose(fp);

    // Recompute counts robustly (since we forced EPS/$ kinds)
    G->numTerminals = 0;
    G->numNonTerminals = 0;
    for (int i = 0; i < G->numSymbols; i++) {
        if (G->symbols[i].kind == SYM_TERMINAL) G->numTerminals++;
        else G->numNonTerminals++;
    }

    return 1;
}

void printGrammar(const Grammar *G, FILE *out) {
    fprintf(out, "Grammar summary:\n");
    fprintf(out, "  Symbols: %d (T=%d, NT=%d)\n", G->numSymbols, G->numTerminals, G->numNonTerminals);
    fprintf(out, "  Productions: %d\n", G->numProds);
    fprintf(out, "  Start symbol: %s\n", (G->startSymbol >= 0) ? G->symbols[G->startSymbol].name : "(none)");
    fprintf(out, "\n");

    for (int i = 0; i < G->numProds; i++) {
        const Production *P = &G->prods[i];
        fprintf(out, "%s ===> ", G->symbols[P->lhs].name);

        for (int a = 0; a < P->numAlts; a++) {
            if (a) fprintf(out, " | ");

            const RHS *r = &P->alts[a];
            for (int j = 0; j < r->rhsLen; j++) {
                fprintf(out, "%s", G->symbols[r->rhs[j]].name);
                if (j + 1 < r->rhsLen) fprintf(out, " ");
            }
        }
        fprintf(out, "\n");
    }
}