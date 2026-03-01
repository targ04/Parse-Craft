/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


#ifndef LEXER_H
#define LEXER_H

#include "lexerDef.h"

// Initialization
void initLexer(twinBuffer *B, FILE *fp, lexerConfig cfg);
void resetLexer(twinBuffer *B);           // reset pointers but keep fp
void closeLexer(twinBuffer *B);           // does not fclose(fp) unless we decide

// As per project Specifications
FILE *getStream(FILE *fp);               
tokenInfo getNextToken(twinBuffer *B);
void removeComments(const char *testcaseFile, const char *cleanFile);

// Buffer helpers
int  nextChar(twinBuffer *B);            // returns unsigned char (0-255) or EOF
void retractChar(twinBuffer *B, int n);  // n=1 or n=2 etc.
void markLexemeBegin(twinBuffer *B);
size_t extractLexeme(twinBuffer *B, char *out, size_t outCap); // returns length

// keyword handling 
TokenType lookupKeyword(const char *lexeme);
bool isKeyword(const char *lexeme);

// ERROR reporting
void reportLexError(const twinBuffer *B, LexErrorCode code, const char *lexemeHint);

// Debug
const char* tokenToString(TokenType t);

#endif