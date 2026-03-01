/*

GROUP 12:
2022B3A71033P       ARJUN NEEKHRA
2022B4A70596P       ARPITA TOMAR
2022B3A70581P       ARVIND ANNAMALAI BALASUBRAMANIAN
2022B3A70604P       MEHUL SRIVASTAVA
2022B2A71101P       S PRANAV KUMAR
2022B3A70453P       TARUN G

*/


#ifndef SET_H
#define SET_H


//generic bitset module
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint64_t *w;     // words
    int nbits;       // universe size
    int nwords;      // = ceil(nbits/64)
} BitSet;

// lifecycle
void setInit(BitSet *s, int nbits);
void setFree(BitSet *s);

// basic ops
void setClearAll(BitSet *s);
void setFillAll(BitSet *s);                 // sets all bits 0..nbits-1
void setCopy(BitSet *dst, const BitSet *src);

bool setAdd(BitSet *s, int x);              // returns true if changed (was 0 -> 1)
bool setRemove(BitSet *s, int x);           // returns true if changed (was 1 -> 0)
bool setContains(const BitSet *s, int x);

bool setIsEmpty(const BitSet *s);
bool setEquals(const BitSet *a, const BitSet *b);

// bulk ops (return true if dst changed)
bool setUnionInto(BitSet *dst, const BitSet *src);       // dst |= src
bool setIntersectInto(BitSet *dst, const BitSet *src);   // dst &= src
bool setDiffInto(BitSet *dst, const BitSet *src);        // dst -= src (dst &= ~src)

// iteration
// Returns smallest element >= start that is present; returns -1 if none.
int setNext(const BitSet *s, int start);

// debugging (prints indices)
void setPrintIdx(const BitSet *s, FILE *out);

#endif