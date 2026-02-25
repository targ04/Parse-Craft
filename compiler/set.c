#include "set.h"
#include <stdlib.h>
#include <string.h>

#define WORD_BITS 64

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return q;
}

static inline int wordIndex(int bit) { return bit / WORD_BITS; }
static inline int bitOffset(int bit) { return bit % WORD_BITS; }

void setInit(BitSet *s, int nbits) {
    s->nbits = nbits;
    s->nwords = (nbits + WORD_BITS - 1) / WORD_BITS;
    s->w = (uint64_t*)xcalloc((size_t)s->nwords, sizeof(uint64_t));
}

void setFree(BitSet *s) {
    if (!s) return;
    free(s->w);
    s->w = NULL;
    s->nbits = 0;
    s->nwords = 0;
}

void setClearAll(BitSet *s) {
    memset(s->w, 0, sizeof(uint64_t) * (size_t)s->nwords);
}

void setFillAll(BitSet *s) {
    for (int i = 0; i < s->nwords; i++) s->w[i] = ~0ULL;

    // clear extra bits beyond nbits (so iteration/equals behave cleanly)
    int extra = s->nwords * WORD_BITS - s->nbits;
    if (extra > 0) {
        uint64_t mask = ~0ULL;
        mask >>= extra; // keep lowest (64-extra) bits
        s->w[s->nwords - 1] &= mask;
    }
}

void setCopy(BitSet *dst, const BitSet *src) {
    if (dst->nbits != src->nbits) {
        dst->nbits = src->nbits;
        dst->nwords = src->nwords;
        dst->w = (uint64_t*)xrealloc(dst->w, sizeof(uint64_t) * (size_t)dst->nwords);
    }
    memcpy(dst->w, src->w, sizeof(uint64_t) * (size_t)src->nwords);
}

bool setAdd(BitSet *s, int x) {
    if (x < 0 || x >= s->nbits) return false;
    int wi = wordIndex(x);
    uint64_t m = 1ULL << bitOffset(x);
    uint64_t before = s->w[wi];
    s->w[wi] |= m;
    return (before != s->w[wi]);
}

bool setRemove(BitSet *s, int x) {
    if (x < 0 || x >= s->nbits) return false;
    int wi = wordIndex(x);
    uint64_t m = 1ULL << bitOffset(x);
    uint64_t before = s->w[wi];
    s->w[wi] &= ~m;
    return (before != s->w[wi]);
}

bool setContains(const BitSet *s, int x) {
    if (x < 0 || x >= s->nbits) return false;
    int wi = wordIndex(x);
    uint64_t m = 1ULL << bitOffset(x);
    return (s->w[wi] & m) != 0ULL;
}

bool setIsEmpty(const BitSet *s) {
    for (int i = 0; i < s->nwords; i++) {
        if (s->w[i] != 0ULL) return false;
    }
    return true;
}

bool setEquals(const BitSet *a, const BitSet *b) {
    if (a->nbits != b->nbits) return false;
    for (int i = 0; i < a->nwords; i++) {
        if (a->w[i] != b->w[i]) return false;
    }
    return true;
}

bool setUnionInto(BitSet *dst, const BitSet *src) {
    bool changed = false;
    for (int i = 0; i < dst->nwords; i++) {
        uint64_t before = dst->w[i];
        dst->w[i] |= src->w[i];
        if (dst->w[i] != before) changed = true;
    }
    return changed;
}

bool setIntersectInto(BitSet *dst, const BitSet *src) {
    bool changed = false;
    for (int i = 0; i < dst->nwords; i++) {
        uint64_t before = dst->w[i];
        dst->w[i] &= src->w[i];
        if (dst->w[i] != before) changed = true;
    }
    return changed;
}

bool setDiffInto(BitSet *dst, const BitSet *src) {
    bool changed = false;
    for (int i = 0; i < dst->nwords; i++) {
        uint64_t before = dst->w[i];
        dst->w[i] &= ~src->w[i];
        if (dst->w[i] != before) changed = true;
    }
    return changed;
}

int setNext(const BitSet *s, int start) {
    if (start < 0) start = 0;
    if (start >= s->nbits) return -1;

    int wi = wordIndex(start);
    int bo = bitOffset(start);

    // mask off bits < start within first word
    uint64_t word = s->w[wi];
    word &= (~0ULL << bo);

    while (wi < s->nwords) {
        if (word != 0ULL) {
            // find least significant set bit
#if defined(__GNUC__) || defined(__clang__)
            int offset = __builtin_ctzll(word);
#else
            // portable fallback
            int offset = 0;
            while (((word >> offset) & 1ULL) == 0ULL) offset++;
#endif
            int ans = wi * WORD_BITS + offset;
            return (ans < s->nbits) ? ans : -1;
        }
        wi++;
        if (wi >= s->nwords) break;
        word = s->w[wi];
    }
    return -1;
}

void setPrintIdx(const BitSet *s, FILE *out) {
    fprintf(out, "{ ");
    for (int x = setNext(s, 0); x != -1; x = setNext(s, x + 1)) {
        fprintf(out, "%d ", x);
    }
    fprintf(out, "}");
}