/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


#ifndef LEXERDEF_H
#define LEXERDEF_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define LEXEME_MAX_LEN 64   
#define TWIN_BUF_SIZE 4096
#define KEYWORD_MAX 32  //keyword list count

// Token Types
typedef enum TokenType{
    TK_EOF=0,

    //identifiers / literals
    TK_ID,
    TK_FUNID,
    TK_RUID,
    TK_FIELDID,
    TK_NUM,
    TK_RNUM,

    //keywords
    TK_WITH,
    TK_PARAMETERS,
    TK_END,
    TK_WHILE,
    TK_UNION,
    TK_ENDUNION,
    TK_DEFINETYPE,
    TK_AS,
    TK_TYPE,
    TK_MAIN,
    TK_GLOBAL,
    TK_PARAMETER,
    TK_LIST,
    TK_INPUT,
    TK_OUTPUT,
    TK_INT,
    TK_REAL,
    TK_ENDWHILE,
    TK_IF,
    TK_THEN,
    TK_ENDIF,
    TK_READ,
    TK_WRITE,
    TK_RETURN,
    TK_CALL,
    TK_RECORD,
    TK_ENDRECORD,
    TK_ELSE,

    // operators/ delimiters
    TK_ASSIGNOP,   // <---
    TK_COMMENT,    // % 
    TK_SQL,        // [
    TK_SQR,        // ]
    TK_COMMA,      // ,
    TK_SEM,        // ;
    TK_COLON,      // :
    TK_DOT,        // .
    TK_OP,         // (
    TK_CL,         // )

    TK_PLUS,       // +
    TK_MINUS,      // -
    TK_MUL,        // *
    TK_DIV,        // /

    TK_AND,        // &&&
    TK_OR,         // @@@
    TK_NOT,        // ~

    TK_LT,         // <
    TK_LE,         // <=
    TK_EQ,         // ==
    TK_GT,         // >
    TK_GE,         // >=
    TK_NE,         // !=

    TK_ERROR
}TokenType;

// a token will either be a NUM or RNUM, we can use a union to store the value
typedef union {
    long long intVal;
    double realVal;
} TokenValue;

// Token info returned to parser
typedef struct tokenInfo {
    TokenType type;     // token type
    int lineNo;         // line number where the lexeme was found
    int errCode;        // error code for better erro reporting

    char lexeme[LEXEME_MAX_LEN];
    TokenValue value;   // numeric value if the token is NUM/RNUM
} tokenInfo;

// Twin Buffer Structure
// We store two buffers; forward pointer moves; refill happens on boundary.
typedef struct twinBuffer {
    FILE *fp;   // file pointer

    char buf1[TWIN_BUF_SIZE];
    char buf2[TWIN_BUF_SIZE];

    // How many bytes are valid in each buffer (<= TWIN_BUF_SIZE)
    size_t len1;
    size_t len2;

    // Which buffer is active for 'forward' pointer: 1 or 2
    int activeBuf;

    // Indices within active buffer
    size_t forward;        // index of next char to read
    size_t lexemeBegin;    // index of lexeme start (within activeBuf) - careful across buffers

    // Track whether EOF reached
    bool eof;

    // Current line
    int lineNo;

    // For robust lexeme extraction across buffers:
    // store absolute stream position-like counters
    uint64_t absForward;       // absolute char offset from file start (monotone)
    uint64_t absLexemeBegin;
} twinBuffer;

// Keyword table entry
typedef struct keywordEntry {
    const char *lexeme;
    TokenType type;
} keywordEntry;

// Lexer config flags
typedef struct lexerConfig {
    bool returnComments;   // usually false
    bool printErrors;      // true in driver
} lexerConfig;

// Error codes
typedef enum LexErrorCode {
    LEXERR_NONE = 0,
    LEXERR_UNKNOWN_SYMBOL,
    LEXERR_ASSIGN_INCOMPLETE,     // "<--", "<-"
    LEXERR_BAD_RNUM,              // e.g., "23.4" (1 digit after dot), "23.45E-" etc.
    LEXERR_BAD_NEQ,               // "!" alone
    LEXERR_BAD_AND,               // "&&" etc.
    LEXERR_BAD_OR,                // "@@" etc.
    LEXERR_TOO_LONG_LEXEME
} LexErrorCode;

#endif 