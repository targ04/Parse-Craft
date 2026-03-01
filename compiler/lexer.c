/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/



#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexerDef.h"

// Keyword table
static const keywordEntry KW[] = {
    {"with", TK_WITH},
    {"parameters", TK_PARAMETERS},
    {"end", TK_END},
    {"while", TK_WHILE},
    {"union", TK_UNION},
    {"endunion", TK_ENDUNION},
    {"definetype", TK_DEFINETYPE},
    {"as", TK_AS},
    {"type", TK_TYPE},
    {"_main", TK_MAIN},
    {"global", TK_GLOBAL},
    {"parameter", TK_PARAMETER},
    {"list", TK_LIST},
    {"input", TK_INPUT},
    {"output", TK_OUTPUT},
    {"int", TK_INT},
    {"real", TK_REAL},
    {"endwhile", TK_ENDWHILE},
    {"if", TK_IF},
    {"then", TK_THEN},
    {"endif", TK_ENDIF},
    {"read", TK_READ},
    {"write", TK_WRITE},
    {"return", TK_RETURN},
    {"call", TK_CALL},
    {"record", TK_RECORD},
    {"endrecord", TK_ENDRECORD},
    {"else", TK_ELSE}
};
static const size_t KW_N = sizeof(KW) / sizeof(KW[0]);
static lexerConfig g_cfg = { .returnComments = false, .printErrors = true };

// Internal helpers
static size_t refillBuffer(FILE *fp, char *buf, size_t cap) {
    return fread(buf, 1, cap, fp);
}

static inline char* activeBufPtr(twinBuffer *B) {
    return (B->activeBuf == 1) ? B->buf1 : B->buf2;
}
static inline size_t activeLen(twinBuffer *B) {
    return (B->activeBuf == 1) ? B->len1 : B->len2;
}
static inline char* otherBufPtr(twinBuffer *B) {
    return (B->activeBuf == 1) ? B->buf2 : B->buf1;
}
static inline size_t otherLen(twinBuffer *B) {
    return (B->activeBuf == 1) ? B->len2 : B->len1;
}

void initLexer(twinBuffer *B, FILE *fp, lexerConfig cfg) {
    memset(B, 0, sizeof(*B));
    B->fp = fp;
    B->activeBuf = 1;
    B->forward = 0;
    B->lexemeBegin = 0;
    B->lineNo = 1;
    B->absForward = 0;
    B->absLexemeBegin = 0;
    B->eof = false;
    g_cfg = cfg;

    B->len1 = refillBuffer(fp, B->buf1, TWIN_BUF_SIZE);
    B->len2 = 0;
    if (B->len1 == 0) B->eof = true;
}

void resetLexer(twinBuffer *B) {
    FILE *fp = B->fp;
    lexerConfig cfg = g_cfg;
    initLexer(B, fp, cfg);
}

void closeLexer(twinBuffer *B) {
    // do not fclose(B->fp) unless your architecture wants it
    (void)B;
}

FILE *getStream(FILE *fp) {
    return fp;
}



// Move to the other buffer and refill it
static void switchAndRefill(twinBuffer *B) {
    if (B->eof) return;

    if (B->activeBuf == 1) {
        // switch to buf2; refill if empty or fully consumed
        if (B->forward >= B->len1) {
            B->len2 = refillBuffer(B->fp, B->buf2, TWIN_BUF_SIZE);
            B->activeBuf = 2;
            B->forward = 0;
            if (B->len2 == 0) B->eof = true;
        }
    } else {
        if (B->forward >= B->len2) {
            B->len1 = refillBuffer(B->fp, B->buf1, TWIN_BUF_SIZE);
            B->activeBuf = 1;
            B->forward = 0;
            if (B->len1 == 0) B->eof = true;
        }
    }
}


int nextChar(twinBuffer *B) {
    // Ensure we have data in active buffer
    if (!B->eof && B->forward >= activeLen(B)) {
        switchAndRefill(B);
    }
    if (B->eof) return EOF;

    char *buf = activeBufPtr(B);
    unsigned char c = (unsigned char)buf[B->forward++];
    B->absForward++;

    if (c == '\n') B->lineNo++;
    return (int)c;
}

/*
 * retractChar: supports retract across buffer boundary (common with lookahead).
 * Assumes we only retract a small number of chars (which is true for this lexer).
 */
void retractChar(twinBuffer *B, int n) {
    while (n-- > 0) {
        if (B->absForward == 0) return;

        if (B->forward > 0) {
            // retract within same buffer
            char *buf = activeBufPtr(B);
            unsigned char prev = (unsigned char)buf[B->forward - 1];
            if (prev == '\n' && B->lineNo > 1) B->lineNo--;
            B->forward--;
            B->absForward--;
        } else {
            // move to the other buffer end (previous chunk)
            // NOTE: This is valid as long as the other buffer still contains the previous chunk.
            if (otherLen(B) == 0) return;

            // switch buffer
            B->activeBuf = (B->activeBuf == 1) ? 2 : 1;
            B->forward = activeLen(B); // move to end
            // now retract within that buffer on next loop iteration
        }
    }
}


void markLexemeBegin(twinBuffer *B) {
    B->lexemeBegin = B->forward;
    B->absLexemeBegin = B->absForward;
}

// Extract lexeme between absLexemeBegin..absForward.
// This is non-trivial across buffers; safest approach:
size_t extractLexeme(twinBuffer *B, char *out, size_t outCap) {
    // Minimal version: only works when lexeme is within current active buffer.
    // You can later upgrade using abs pointers + storing a rolling lexeme build buffer.
    size_t start = B->lexemeBegin;
    size_t end   = B->forward;
    size_t len = (end > start) ? (end - start) : 0;

    if (len + 1 > outCap) len = outCap - 1;

    char *buf = activeBufPtr(B);
    memcpy(out, &buf[start], len);
    out[len] = '\0';
    return len;
}

// Keyword handling
TokenType lookupKeyword(const char *lexeme) {
    // Linear search is fine (small KW table). Could also sort + binary search.
    for (size_t i = 0; i < KW_N; i++) {
        if (strcmp(lexeme, KW[i].lexeme) == 0) return KW[i].type;
    }
    return TK_ERROR; // means "not a keyword" in our usage below
}

bool isKeyword(const char *lexeme) {
    return lookupKeyword(lexeme) != TK_ERROR;
}

// ERROR reporting
static const char* lexErrCodeToString(LexErrorCode code) {
    switch (code) {
        case LEXERR_NONE:              return "LEXERR_NONE";
        case LEXERR_UNKNOWN_SYMBOL:    return "LEXERR_UNKNOWN_SYMBOL";
        case LEXERR_ASSIGN_INCOMPLETE: return "LEXERR_ASSIGN_INCOMPLETE";
        case LEXERR_BAD_RNUM:          return "LEXERR_BAD_RNUM";
        case LEXERR_BAD_NEQ:           return "LEXERR_BAD_NEQ";
        case LEXERR_BAD_AND:           return "LEXERR_BAD_AND";
        case LEXERR_BAD_OR:            return "LEXERR_BAD_OR";
        case LEXERR_TOO_LONG_LEXEME:   return "LEXERR_TOO_LONG_LEXEME";
        default:                       return "LEXERR_UNKNOWN";
    }
}
void reportLexError(const twinBuffer *B, LexErrorCode code, const char *lexemeHint) {
    if (!g_cfg.printErrors) return;
    fprintf(stderr, "[LEXERR] line %d errCode %s lexeme '%s'\n",
            B->lineNo, lexErrCodeToString(code), (lexemeHint ? lexemeHint : ""));
}

const char* tokenToString(TokenType t) {
    switch (t) {
        case TK_EOF: return "TK_EOF";

        // identifiers / literals
        case TK_ID: return "TK_ID";
        case TK_FUNID: return "TK_FUNID";
        case TK_RUID: return "TK_RUID";
        case TK_FIELDID: return "TK_FIELDID";
        case TK_NUM: return "TK_NUM";
        case TK_RNUM: return "TK_RNUM";

        // keywords
        case TK_WITH: return "TK_WITH";
        case TK_PARAMETERS: return "TK_PARAMETERS";
        case TK_END: return "TK_END";
        case TK_WHILE: return "TK_WHILE";
        case TK_UNION: return "TK_UNION";
        case TK_ENDUNION: return "TK_ENDUNION";
        case TK_DEFINETYPE: return "TK_DEFINETYPE";
        case TK_AS: return "TK_AS";
        case TK_TYPE: return "TK_TYPE";
        case TK_MAIN: return "TK_MAIN";
        case TK_GLOBAL: return "TK_GLOBAL";
        case TK_PARAMETER: return "TK_PARAMETER";
        case TK_LIST: return "TK_LIST";
        case TK_INPUT: return "TK_INPUT";
        case TK_OUTPUT: return "TK_OUTPUT";
        case TK_INT: return "TK_INT";
        case TK_REAL: return "TK_REAL";
        case TK_ENDWHILE: return "TK_ENDWHILE";
        case TK_IF: return "TK_IF";
        case TK_THEN: return "TK_THEN";
        case TK_ENDIF: return "TK_ENDIF";
        case TK_READ: return "TK_READ";
        case TK_WRITE: return "TK_WRITE";
        case TK_RETURN: return "TK_RETURN";
        case TK_CALL: return "TK_CALL";
        case TK_RECORD: return "TK_RECORD";
        case TK_ENDRECORD: return "TK_ENDRECORD";
        case TK_ELSE: return "TK_ELSE";

        // operators / delimiters
        case TK_ASSIGNOP: return "TK_ASSIGNOP";
        case TK_COMMENT: return "TK_COMMENT";
        case TK_SQL: return "TK_SQL";
        case TK_SQR: return "TK_SQR";
        case TK_COMMA: return "TK_COMMA";
        case TK_SEM: return "TK_SEM";
        case TK_COLON: return "TK_COLON";
        case TK_DOT: return "TK_DOT";
        case TK_OP: return "TK_OP";
        case TK_CL: return "TK_CL";

        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_MUL: return "TK_MUL";
        case TK_DIV: return "TK_DIV";

        case TK_AND: return "TK_AND";
        case TK_OR: return "TK_OR";
        case TK_NOT: return "TK_NOT";

        case TK_LT: return "TK_LT";
        case TK_LE: return "TK_LE";
        case TK_EQ: return "TK_EQ";
        case TK_GT: return "TK_GT";
        case TK_GE: return "TK_GE";
        case TK_NE: return "TK_NE";

        case TK_ERROR: return "TK_ERROR";
        default: return "TK_UNKNOWN";
    }
}

// remove comments % blah blah blah
 void removeComments(const char *testcaseFile, const char *cleanFile) {
    FILE *in = fopen(testcaseFile, "r");
    if (!in) return;
    FILE *out = fopen(cleanFile, "w");
    if (!out) { fclose(in); return; }

    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '%') {
            // skip until end of line OR EOF
            while ((c = fgetc(in)) != EOF && c != '\n') {}
            if (c == '\n') fputc('\n', out);
        } else {
            fputc(c, out);
        }
    }
    fclose(in);
    fclose(out);
}

// small helper for building token
static tokenInfo makeTok(TokenType t, int line, const char *lex) {
    tokenInfo tk;
    memset(&tk, 0, sizeof(tk));
    tk.type = t;
    tk.lineNo = line;
    tk.errCode = LEXERR_NONE;
    if (lex) {
        strncpy(tk.lexeme, lex, LEXEME_MAX_LEN - 1);
        tk.lexeme[LEXEME_MAX_LEN - 1] = '\0';
    } else {
        tk.lexeme[0] = '\0';
    }
    return tk;
}

// getNextToken ---> This is where our hardworked + handdrawn DFA resides
tokenInfo getNextToken(twinBuffer *B) {
    tokenInfo tk;
    memset(&tk, 0, sizeof(tk));
    tk.type = TK_ERROR;
    tk.lineNo = B->lineNo;
    tk.lexeme[0] = '\0';
    tk.errCode = LEXERR_NONE;

    // Skip whitespace and comments (% ... \n)
    int c;
    while (1) {
        c = nextChar(B);
        if (c == EOF) return makeTok(TK_EOF, B->lineNo, "EOF");
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;

        if (c == '%') {
            // ignore comment
            while ((c = nextChar(B)) != EOF && c != '\n') {}
            if (c=='\n')return makeTok(TK_COMMENT, B->lineNo-1, "COMMENT");
            else return makeTok(TK_COMMENT, B->lineNo, "COMMENT");
        }
        break;
    }


    // We are at first non-space non-comment character
    markLexemeBegin(B);
    //tk.lineNo = B->lineNo;
    int startLine = B->lineNo; 

    // Handling tokens one by one

    // token starting with "<" ,i.e., "<---", "<=", "<"
    if (c=='<'){
        int c2= nextChar(B); //next character
        if(c2=='='){    // "<="
            return makeTok(TK_LE, startLine, "<=");
        }
        if (c2 == '-') {
            int c3 = nextChar(B);
            if (c3 == '-') {
                int c4 = nextChar(B);
                if (c4 == '-') {
                    return makeTok(TK_ASSIGNOP, startLine, "<---");
                } 
                // We saw "<--" followed by something else (c4 is not '-')
                // Errata-consistent: treat "<--" as lexical error (incomplete assignop).
                if (c4 != EOF) retractChar(B, 1);
                tk = makeTok(TK_ERROR, startLine, "<--");
                tk.errCode = LEXERR_ASSIGN_INCOMPLETE;
                reportLexError(B, tk.errCode, tk.lexeme);
                return tk;
            } 
            // We saw "<-" followed by something not '-' (c3 is not '-')
            // Errata: "<-" is lexical error.
            if (c3 != EOF) retractChar(B, 1);
            tk = makeTok(TK_ERROR, startLine, "<-");
            tk.errCode = LEXERR_ASSIGN_INCOMPLETE;
            reportLexError(B, tk.errCode, tk.lexeme);
            return tk;
        }
        // Just '<'
        if (c2 != EOF) retractChar(B, 1);
        return makeTok(TK_LT, startLine, "<");
    }

    // Token "=="
    if(c=='='){
        int c2 = nextChar(B);
        if (c2 == '=') {
            return makeTok(TK_EQ, startLine, "==");
        }
        // '=' alone is an error
        if (c2 != EOF) retractChar(B, 1);
        tk = makeTok(TK_ERROR, startLine, "=");
        tk.errCode = LEXERR_UNKNOWN_SYMBOL;
        reportLexError(B, tk.errCode, tk.lexeme);
        return tk;
    }

    // != or '!' error
    if (c == '!') {
        int c2 = nextChar(B);
        if (c2 == '=') {
            return makeTok(TK_NE, startLine, "!=");
        }
        // '!' alone should be lexical error per spec examples
        if (c2 != EOF) retractChar(B, 1);
        tk = makeTok(TK_ERROR, startLine, "!");
        tk.errCode = LEXERR_BAD_NEQ;
        reportLexError(B, tk.errCode, tk.lexeme);
        return tk;
    }

    // >= or >
    if (c == '>') {
        int c2 = nextChar(B);
        if (c2 == '=') {
            return makeTok(TK_GE, startLine, ">=");
        }
        if (c2 != EOF) retractChar(B, 1);
        return makeTok(TK_GT, startLine, ">");
    }

    // &&& or errors like &&
    if (c == '&') {
        int c2 = nextChar(B);
        if(c2=='&'){
            int c3 = nextChar(B);
            if (c3 == '&') {
                return makeTok(TK_AND, startLine, "&&&");
            }
            // It is "&&" followed by a non-'&' character
            if (c3 != EOF) retractChar(B, 1);
            tk = makeTok(TK_ERROR, startLine, "&&");
            tk.errCode = LEXERR_BAD_AND;
            reportLexError(B, tk.errCode, "&&");
            return tk;
        }

        // Single '&' -> lexical error
        if (c2 != EOF) retractChar(B, 1);
        tk = makeTok(TK_ERROR, startLine, "&");
        tk.errCode = LEXERR_BAD_AND;
        reportLexError(B, tk.errCode, "&");
        return tk;
    }

    // @@@ or errors like @@
    if (c == '@') {
        int c2 = nextChar(B);
        if(c2=='@'){
            int c3 = nextChar(B);
            if (c3 == '@') {
                return makeTok(TK_OR, startLine, "@@@");  /* fixed: was "@@" */
            }
            // It is "@@" followed by a non-'@' character
            if (c3 != EOF) retractChar(B, 1);
            tk = makeTok(TK_ERROR, startLine, "@@");
            tk.errCode = LEXERR_BAD_OR;
            reportLexError(B, tk.errCode, "@@");
            return tk;
        }
        // Single '@' -> lexical error
        if (c2 != EOF) retractChar(B, 1);
        tk = makeTok(TK_ERROR, startLine, "@");
        tk.errCode = LEXERR_BAD_OR;
        reportLexError(B, tk.errCode, "@");
        return tk;
    }

    // single-char punctuation / operators
    switch (c) {
        case '[': return makeTok(TK_SQL,   startLine, "[");
        case ']': return makeTok(TK_SQR,   startLine, "]");
        case ',': return makeTok(TK_COMMA, startLine, ",");
        case ';': return makeTok(TK_SEM,   startLine, ";");
        case ':': return makeTok(TK_COLON, startLine, ":");
        case '.': return makeTok(TK_DOT,   startLine, ".");
        case '(': return makeTok(TK_OP,    startLine, "(");
        case ')': return makeTok(TK_CL,    startLine, ")");
        case '+': return makeTok(TK_PLUS,  startLine, "+");
        case '-': return makeTok(TK_MINUS, startLine, "-");
        case '*': return makeTok(TK_MUL,   startLine, "*");
        case '/': return makeTok(TK_DIV,   startLine, "/");
        case '~': return makeTok(TK_NOT,   startLine, "~");
        default: break;
    }

    // Numbers: NUM / RNUM / RNUM with exponent
    if(isdigit((unsigned char)c)){
        // NUM: [0-9]+
        // RNUM: [0-9]+ '.' [0-9][0-9] (exactly 2 digits)
        // exponent: ... 'E' ('+'|'-'|epsilon?) [0-9][0-9] (exactly 2 digits)
        
        // 
        char lex[LEXEME_MAX_LEN];
        int k=0;
        lex[k++]= (char)c;
        int cNext = nextChar(B);
        while(cNext != EOF && isdigit((unsigned char)cNext)){
            if(k< LEXEME_MAX_LEN-1) lex[k++]= (char)cNext;
            cNext = nextChar(B);
        }
        if(cNext=='.'){
            if (k < LEXEME_MAX_LEN-1) lex[k++] = '.';
            int decimal= nextChar(B);
            if(decimal!=EOF && isdigit((unsigned char)decimal)){
                if (k < LEXEME_MAX_LEN-1) lex[k++] = (char)decimal;
                decimal= nextChar(B);
                if(decimal!=EOF && isdigit((unsigned char)decimal)){
                    if (k < LEXEME_MAX_LEN-1) lex[k++] = (char)decimal;
                    int exponent= nextChar(B);
                    if (exponent=='E'){
                        if(k< LEXEME_MAX_LEN-1) lex[k++]='E';
                        int op=nextChar(B);
                        if(op== '+'|| op=='-'){
                            if(k< LEXEME_MAX_LEN-1) lex[k++]=(char)op;
                            op=nextChar(B);
                        }
                        if(op!=EOF && isdigit((unsigned char)op)){
                            if(k< LEXEME_MAX_LEN-1) lex[k++]=(char)op;
                            op=nextChar(B);
                            if(op!=EOF && isdigit((unsigned char)op)){
                                if(k< LEXEME_MAX_LEN-1) lex[k++]=(char)op;
                                lex[k]= '\0';
                                tk = makeTok(TK_RNUM, startLine, lex);
                                tk.value.realVal = atof(lex);
                                return tk;
                            }
                        }
                        if(op!= EOF) retractChar(B, 1);
                        lex[k]='\0';
                        tk = makeTok(TK_ERROR, startLine, lex);
                        tk.errCode = LEXERR_BAD_RNUM;
                        reportLexError(B, tk.errCode, tk.lexeme);
                        return tk;
                    }
                    // We have exactly two digits after dot -> base RNUM
                    if(exponent!=EOF)retractChar(B, 1);
                    lex[k]= '\0';
                    tk = makeTok(TK_RNUM, startLine, lex);
                    tk.value.realVal = atof(lex);
                    return tk;
                }
            }
            if(decimal!=EOF) retractChar(B, 1);
            lex[k]= '\0';
            tk = makeTok(TK_ERROR, startLine, lex); // contains "numbers."
            tk.errCode = LEXERR_BAD_RNUM;
            reportLexError(B, tk.errCode, tk.lexeme);
            return tk;
        }
        if(cNext!=EOF) retractChar(B, 1);
        lex[k]= '\0';
        tk = makeTok(TK_NUM, startLine, lex);
        tk.value.intVal= atoll(lex);
        return tk;
    }

    // Identifiers: FUNID, RUID, ID, FIELDID, and keywords
    // FUNID: starts with '_'
    if (c == '_') {
        // DFA per spec: _[a-zA-Z]+ [0-9]*
        // But special-case: "_main" is keyword TK_MAIN (your keyword table already maps it)
        // 
        char lex[LEXEME_MAX_LEN];
        int k = 0;
        lex[k++] = '_';

        int ch = nextChar(B);
        if (ch == EOF || !isalpha((unsigned char)ch)) {
            if (ch != EOF) retractChar(B, 1);
            lex[k] = '\0';
            tk = makeTok(TK_ERROR, startLine, lex);
            tk.errCode = LEXERR_UNKNOWN_SYMBOL;
            reportLexError(B, tk.errCode, tk.lexeme);
            return tk;
        }
        while (ch != EOF && (isalpha((unsigned char)ch) || isdigit((unsigned char)ch))) {
            if (k < LEXEME_MAX_LEN - 1) lex[k++] = (char)ch;
            ch = nextChar(B);
        }
        if (ch != EOF) retractChar(B, 1);
        lex[k] = '\0';

        // keyword special-case (_main)
        TokenType kwt = lookupKeyword(lex);
        if (kwt != TK_ERROR) return makeTok(kwt, startLine, lex);

        // enforce max length (spec says <= 30; your LEXEME_MAX_LEN should be >= 31)
        if ((int)strlen(lex) > 30) {
            tk = makeTok(TK_ERROR, startLine, lex);
            tk.errCode = LEXERR_TOO_LONG_LEXEME;
            reportLexError(B, tk.errCode, tk.lexeme);
            return tk;
        }
        return makeTok(TK_FUNID, startLine, lex);
    }

    // RUID: starts with '#'
    if (c == '#') {
        // DFA: #[a-z]+
        // 
        char lex[LEXEME_MAX_LEN];
        int k = 0;
        lex[k++] = '#';

        int ch = nextChar(B);
        if (ch == EOF || !islower((unsigned char)ch)) {
            if (ch != EOF) retractChar(B, 1);
            lex[k] = '\0';
            tk = makeTok(TK_ERROR, startLine, lex);
            tk.errCode = LEXERR_UNKNOWN_SYMBOL;
            reportLexError(B, tk.errCode, tk.lexeme);
            return tk;
        }

        while (ch != EOF && islower((unsigned char)ch)) {
            if (k < LEXEME_MAX_LEN - 1) lex[k++] = (char)ch;
            ch = nextChar(B);
        }
        if (ch != EOF) retractChar(B, 1);
        lex[k] = '\0';
        return makeTok(TK_RUID, startLine, lex);
    }

    // ID: [b-d][2-7][b-d]*[2-7]*  (length 2..20)
    // FIELDID / keyword: [a-z]+
    if (islower((unsigned char)c)) {
        // Peek second char to decide if it is ID (only if first in {b,c,d} and second in {2..7})
        if (c == 'b' || c == 'c' || c == 'd') {
            int c2 = nextChar(B);
            if (c2 != EOF && (c2 >= '2' && c2 <= '7')) {
                // It's an ID candidate.
                char lex[LEXEME_MAX_LEN];
                int k = 0;
                lex[k++] = (char)c;
                lex[k++] = (char)c2;

                int ch = nextChar(B);

                // consume [b-d]*
                while (ch != EOF && ((ch >= 'b' && ch <= 'd'))) {
                    if (k < LEXEME_MAX_LEN - 1) lex[k++] = (char)ch;
                    ch = nextChar(B);
                }
                // consume [2-7]*
                while (ch != EOF && ((ch >= '2' && ch <= '7'))) {
                    if (k < LEXEME_MAX_LEN - 1) lex[k++] = (char)ch;
                    ch = nextChar(B);
                }
                if (ch != EOF) retractChar(B, 1);
                lex[k] = '\0';

                int len = (int)strlen(lex);
                if (len < 2 || len > 20) {
                    tk = makeTok(TK_ERROR, startLine, lex);
                    tk.errCode = LEXERR_TOO_LONG_LEXEME;
                    reportLexError(B, tk.errCode, tk.lexeme);
                    return tk;
                }

                return makeTok(TK_ID, startLine, lex);
            }

            // Not an ID; put back c2 and fall through to FIELDID/keyword
            if (c2 != EOF) retractChar(B, 1);
        }
        // FIELDID / keyword: [a-z]+
        char lex[LEXEME_MAX_LEN];
        int k = 0;
        lex[k++] = (char)c;

        int ch = nextChar(B);
        while (ch != EOF && islower((unsigned char)ch)) {
            if (k < LEXEME_MAX_LEN - 1) lex[k++] = (char)ch;
            ch = nextChar(B);
        }
        if (ch != EOF) retractChar(B, 1);
        lex[k] = '\0';

        TokenType kwt = lookupKeyword(lex);
        if (kwt != TK_ERROR) return makeTok(kwt, startLine, lex);

        return makeTok(TK_FIELDID, startLine, lex);
    }


    // Unknown character => lexical error
    tk.errCode = LEXERR_UNKNOWN_SYMBOL;
    tk.lexeme[0] = (char)c;
    tk.lexeme[1] = '\0';
    reportLexError(B, tk.errCode, tk.lexeme);
    tk.type = TK_ERROR;
    return tk;
}