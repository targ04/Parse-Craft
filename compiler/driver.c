/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


// driver.c
// Single-folder driver as per project menu specification (0-4)

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "lexer.h"
#include "lexerDef.h"

#include "grammar.h"
#include "set.h"
#include "firstfollow.h"
#include "parser.h"

/* Print a single lex error in the canonical format used by Option 2 */
static void printLexErrorLine(const tokenInfo *tk) {
    switch ((LexErrorCode)tk->errCode) {
        case LEXERR_TOO_LONG_LEXEME:
            printf("Line No. %-4d Error: Variable Identifier is longer than the prescribed length of 20 characters.\n", tk->lineNo);
            break;
        case LEXERR_UNKNOWN_SYMBOL:
            printf("Line No. %-4d Error: Unknown Symbol <%s>\n", tk->lineNo, tk->lexeme);
            break;
        case LEXERR_BAD_AND:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
        case LEXERR_BAD_OR:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
        case LEXERR_ASSIGN_INCOMPLETE:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
        case LEXERR_BAD_RNUM:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
        case LEXERR_BAD_NEQ:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
        default:
            printf("Line No. %-4d Error: Unknown pattern <%s>\n", tk->lineNo, tk->lexeme);
            break;
    }
}

// Banner / status 
static void printImplementationStatus(void) {
    printf("===============================================================\n");
    printf("Compiler Project Driver\n");
    printf("Implementation Status:\n");
    printf("  (a) FIRST and FOLLOW set automated: YES\n");
    printf("  (c) Both lexical and syntax analysis modules implemented: YES\n");
    printf("  Parse table creation: YES\n");
    printf("  Parse tree construction: YES (built inside parser module)\n");
    printf("===============================================================\n\n");
}

// Build parser prerequisites (Grammar, FIRST, FOLLOW, ParseTable) 
// Returns true on success; outputs allocated structures.
// Caller must cleanup using cleanupParserPrereqs.
typedef struct {
    Grammar G;
    BitSet *FIRST;
    BitSet *FOLLOW;
    ParseTable T;
    bool inited;
} ParserPrereqs;

static void cleanupParserPrereqs(ParserPrereqs *P) {
    if (!P || !P->inited) return;

    freeParseTable(&P->T);

    if (P->FIRST && P->FOLLOW) {
        freeFirstFollowArrays(&P->G, P->FIRST, P->FOLLOW);
    }
    free(P->FIRST);
    free(P->FOLLOW);

    freeGrammar(&P->G);

    memset(P, 0, sizeof(*P));
}

static bool buildParserPrereqs(ParserPrereqs *P, const char *grammarFile, FILE *conflictOut) {
    memset(P, 0, sizeof(*P));
    initGrammar(&P->G);

    if (!readGrammar(&P->G, grammarFile)) {
        fprintf(stderr, "ERROR: Failed to read grammar file: %s\n", grammarFile);
        freeGrammar(&P->G);
        return false;
    }

    P->FIRST  = (BitSet*)malloc(sizeof(BitSet) * (size_t)P->G.numSymbols);
    P->FOLLOW = (BitSet*)malloc(sizeof(BitSet) * (size_t)P->G.numSymbols);
    if (!P->FIRST || !P->FOLLOW) {
        fprintf(stderr, "ERROR: Out of memory allocating FIRST/FOLLOW\n");
        free(P->FIRST); free(P->FOLLOW);
        freeGrammar(&P->G);
        return false;
    }

    initFirstFollowArrays(&P->G, P->FIRST, P->FOLLOW);
    computeFIRST(&P->G, P->FIRST);
    computeFOLLOW(&P->G, P->FIRST, P->FOLLOW);

    //printFIRST(&P->G, P->FIRST, stdout);
    //printFOLLOW(&P->G, P->FOLLOW, stdout);
    initParseTable(&P->G, &P->T);
    ParseTableStats st = createParseTable(&P->G, P->FIRST, P->FOLLOW, &P->T, conflictOut);
    fprintf(stdout, "ParseTable: filled=%d conflicts=%d\n", st.numFilled, st.numConflicts);
    //printParseTable(&P->G, &P->T, stdout);
    P->inited = true;
    return true;
}

// Option 1: remove comments and print clean code 
static void optionRemoveCommentsPrint(const char *sourceFile) {
    // removeComments writes to a file; we create a temp file then print it
    const char *tmpClean = "___clean_no_comments.tmp";

    removeComments(sourceFile, tmpClean);

    FILE *fp = fopen(tmpClean, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Could not open temporary clean file: %s\n", tmpClean);
        return;
    }

    int ch;
    while ((ch = fgetc(fp)) != EOF) putchar(ch);
    fclose(fp);

    // best-effort cleanup
    remove(tmpClean);
}

// Option 2: print token list
// Format per spec: each token on a new line with lexeme and line number.
// Comments ARE included (TK_COMMENT) as the sample output shows them.
// Errors are printed in a readable format (not as table rows).
static void optionPrintTokenList(const char *sourceFile) {
    FILE *fp = fopen(sourceFile, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Could not open source file: %s\n", sourceFile);
        return;
    }

    // returnComments = true: TK_COMMENT appears in the stream (sample output shows them)
    // printErrors   = false: we handle error output ourselves to avoid two formats
    lexerConfig cfg = {.returnComments = true, .printErrors = false};
    twinBuffer B;
    initLexer(&B, fp, cfg);

    printf("%-10s  %-26s  %s\n", "Line No.", "Lexeme", "Token");
    printf("--------------------------------------------------------------------\n");

    while (1) {
        tokenInfo tk = getNextToken(&B);

        if (tk.type == TK_ERROR) {
            printLexErrorLine(&tk);
        } else {
            printf("%-10d  %-26s  %s\n",
                   tk.lineNo,
                   (tk.type == TK_EOF) ? "EOF" : tk.lexeme,
                   tokenToString(tk.type));
        }

        if (tk.type == TK_EOF) break;
    }

    closeLexer(&B);
    fclose(fp);
}

// Option 3: parse + print errors to console + parse tree to file 
static void optionParseAndPrintTree(
    const char *sourceFile,
    const char *grammarFile,
    const char *parseTreeOutFile
) {
    // Build grammar/FIRST/FOLLOW/table (independent for this option)
    ParserPrereqs P;
    if (!buildParserPrereqs(&P, grammarFile, stdout)) {
        return;
    }

    FILE *fp = fopen(sourceFile, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Could not open source file: %s\n", sourceFile);
        cleanupParserPrereqs(&P);
        return;
    }

    // printErrors=false: parser handles all error reporting to stdout (no duplicate stderr)
    lexerConfig cfg = {.returnComments = false, .printErrors = false};
    twinBuffer B;
    initLexer(&B, fp, cfg);

    bool ok = true;
    ParseTreeNode *root = parseInputSourceCode(&B, &P.G, &P.T, P.FIRST, P.FOLLOW, stdout, &ok);

    // Spec mandates this exact message on successful parse
    if (ok) {
        printf("\nInput source code is syntactically correct.\n");
    } else {
        printf("\nParsing completed. Syntax errors reported above.\n");
    }

    // Always write the (possibly partial) parse tree — spec says parser recovers and
    // continues building the tree even when there are syntax errors.
    FILE *out = fopen(parseTreeOutFile, "w");
    if (!out) {
        fprintf(stderr, "ERROR: Could not open parse tree output file: %s\n", parseTreeOutFile);
        if (root) {
            // fallback: print to console
            printParseTree(&P.G, root, stdout);
        }
    } else {
        if (root) {
            printParseTree(&P.G, root, out);
            printf(ok?"Parse tree written to: %s\n": "Partial parse tree for syntactically incorrect code written to: %s\n", parseTreeOutFile);
        }
        fclose(out);
    }
    freeParseTree(root);
    root = NULL;

    

    closeLexer(&B);
    fclose(fp);

    cleanupParserPrereqs(&P);
}

// Option 4: timing lexer+parser syntactic verification 
static void optionTimingLexerParser(
    const char *sourceFile,
    const char *grammarFile
) {
    clock_t start_time, end_time;
    double total_CPU_time, total_CPU_time_in_seconds;

    start_time = clock();

    ParserPrereqs P;
    if (!buildParserPrereqs(&P, grammarFile, NULL)) {
        return;
    }

    FILE *fp = fopen(sourceFile, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Could not open source file: %s\n", sourceFile);
        cleanupParserPrereqs(&P);
        return;
    }

    // printErrors=false: parser wrapper handles error reporting; timing option just needs counts
    lexerConfig cfg = {.returnComments = false, .printErrors = false};
    twinBuffer B;
    initLexer(&B, fp, cfg);

    bool ok = true;
    ParseTreeNode *root = parseInputSourceCode(&B, &P.G, &P.T, P.FIRST, P.FOLLOW, stdout, &ok);

    // We don't need the tree for timing; free it
    freeParseTree(root);

    closeLexer(&B);
    fclose(fp);

    cleanupParserPrereqs(&P);

    end_time = clock();
    total_CPU_time = (double)(end_time - start_time);
    total_CPU_time_in_seconds = total_CPU_time / CLOCKS_PER_SEC;

    printf("\nTiming (lexer + parser syntactic verification):\n");
    printf("  total_CPU_time            = %.0f clock ticks\n", total_CPU_time);
    printf("  total_CPU_time_in_seconds = %.6f seconds\n", total_CPU_time_in_seconds);
}

// main
static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <source_file> [parse_tree_output_file]\n\n",
        prog
    );
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char *sourceFile = argv[1];
    const char *grammarFile = "grammar.txt";
    const char *parseTreeOut = argv[2];

    printImplementationStatus();

    int choice = -1;
    while (1) {
        printf("\nChoose an option:\n");
        printf("0 : Exit\n");
        printf("1 : Remove comments and print comment-free code\n");
        printf("2 : Print token list generated by lexer\n");
        printf("3 : Parse to verify syntactic correctness + print parse tree to file\n");
        printf("4 : Print total time taken by lexer+parser (syntactic verification)\n");
        printf("Enter option: ");
        fflush(stdout);

        if (scanf("%d", &choice) != 1) {
            // consume bad input
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == 0) {
            printf("Exiting...\n");
            break;
        }

        switch (choice) {
            case 1:
                optionRemoveCommentsPrint(sourceFile);
                break;

            case 2:
                optionPrintTokenList(sourceFile);
                break;

            case 3:
                optionParseAndPrintTree(sourceFile, grammarFile, parseTreeOut);
                break;

            case 4:
                optionTimingLexerParser(sourceFile, grammarFile);
                break;

            default:
                printf("Invalid option. Please choose 0-4.\n");
                break;
        }
    }
    return 0;
}