/* Urdu text as the phonemes the engine speaks.

   The same job tools/urdu/phones.py does, and deliberately none of the same
   work. Every piece of phonetics -- the IPA mapping, the doubling that makes
   a long vowel, the stress rule that reads syllable weight -- was done once,
   offline, and its answer is in urdu_lex.c. What is left here is what cannot
   be written down in advance: splitting text into words, normalising how a
   word is spelled, reading numbers, and putting the sentence back together.

   Why it has to be here at all. SAPI hands the engine letters. Urdu does not
   write its short vowels, so no rule reading the spelling can know them:
   kitab is k-t-a-b on the page and kitaab in the mouth. Until this, the
   voice a person installed guessed at the vowel in every word while a
   lexicon that knew it sat unused on disk. */

#include <stdlib.h>
#include <string.h>

#include "urdu_lex.h"

/* How much of the blob is the word: the phonemes start after its nought. */
static const char *phones_of(int32_t at)
{
    const char *w = urdu_lex_blob + at;

    return w + strlen(w) + 1;
}

/* The word at an offset, which is the blob at that offset: the pairs are
   stored word first. */
static const char *word_at(int32_t at)
{
    return urdu_lex_blob + at;
}

/* What the lexicon says, or null.

   The index is sorted by the word as Python sorts strings, which is by code
   point, and UTF-8 compares byte for byte in the same order -- so strcmp
   over the raw bytes finds exactly what Python would have found. That
   equivalence is the whole reason this can be a binary search rather than a
   hash of something. */
static const char *look_up(const char *word)
{
    int32_t lo = 0, hi = urdu_lex_count - 1;

    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(word, word_at(urdu_lex_index[mid]));

        if (cmp == 0)
            return phones_of(urdu_lex_index[mid]);
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return 0;
}

/* One code point out of UTF-8, and how many bytes it was. Malformed input
   comes back as the single byte it started with, which keeps this total:
   text from a screen reader is not always what it claims to be, and a
   sentence that cannot be decoded should still be spoken as far as it goes
   rather than dropped. */
static uint32_t decode(const char *s, int *len)
{
    const unsigned char *u = (const unsigned char *)s;

    if (u[0] < 0x80) { *len = 1; return u[0]; }
    if ((u[0] & 0xe0) == 0xc0 && (u[1] & 0xc0) == 0x80) {
        *len = 2;
        return (uint32_t)(u[0] & 0x1f) << 6 | (u[1] & 0x3f);
    }
    if ((u[0] & 0xf0) == 0xe0 && (u[1] & 0xc0) == 0x80 &&
        (u[2] & 0xc0) == 0x80) {
        *len = 3;
        return (uint32_t)(u[0] & 0x0f) << 12 | (uint32_t)(u[1] & 0x3f) << 6 |
               (u[2] & 0x3f);
    }
    if ((u[0] & 0xf8) == 0xf0 && (u[1] & 0xc0) == 0x80 &&
        (u[2] & 0xc0) == 0x80 && (u[3] & 0xc0) == 0x80) {
        *len = 4;
        return (uint32_t)(u[0] & 0x07) << 18 | (uint32_t)(u[1] & 0x3f) << 12 |
               (uint32_t)(u[2] & 0x3f) << 6 | (u[3] & 0x3f);
    }
    *len = 1;
    return u[0];
}

static void encode(uint32_t c, char **out)
{
    char *p = *out;

    if (c < 0x80) {
        *p++ = (char)c;
    } else if (c < 0x800) {
        *p++ = (char)(0xc0 | c >> 6);
        *p++ = (char)(0x80 | (c & 0x3f));
    } else {
        *p++ = (char)(0xe0 | c >> 12);
        *p++ = (char)(0x80 | (c >> 6 & 0x3f));
        *p++ = (char)(0x80 | (c & 0x3f));
    }
    *out = p;
}

/* Letters that arrive as their Arabic or Persian look-alikes, because that
   is what an Arabic keyboard gives and what much text on the web carries.
   The lexicon is spelled the Urdu way, so a word written the other way would
   miss it for no reason. The same table as SAME in tools/urdu/phones.py, and
   it has to stay the same table. Nought means the character is dropped: the
   vowel points, shadda, sukun and the kashida, none of which the lexicon
   spells and all of which stop a word matching it. */
static uint32_t normalise(uint32_t c)
{
    switch (c) {
    case 0x0643: return 0x06a9;   /* kaf -> keheh */
    case 0x064a: return 0x06cc;   /* yeh -> farsi yeh */
    case 0x0649: return 0x06cc;   /* alef maksura -> farsi yeh */
    case 0x0647: return 0x06c1;   /* heh -> heh goal */
    case 0x0629: return 0x06c1;   /* teh marbuta -> heh goal */
    case 0x06c3: return 0x06c1;
    case 0x064e: case 0x064f: case 0x0650:
    case 0x0651: case 0x0652: case 0x0640:
        return 0;
    default: return c;
    }
}

static int is_punct(uint32_t c)
{
    return c == 0x060c || c == 0x061f || c == 0x06d4 ||
           c == '.' || c == ',' || c == '!' || c == '?' || c == ';' ||
           c == ':' || c == '"' || c == 0x27 || c == '(' || c == ')';
}

/* The three sets of digits Urdu text mixes freely: ASCII, Urdu's own, and
   Arabic's, which look the same in most fonts and are different characters.
   Minus one for anything that is not a digit. */
static int digit_of(uint32_t c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 0x06f0 && c <= 0x06f9) return (int)(c - 0x06f0);
    if (c >= 0x0660 && c <= 0x0669) return (int)(c - 0x0660);
    return -1;
}

/* A growing string, because how long the answer is cannot be known before it
   is built and guessing wrong either wastes a megabyte or truncates a
   sentence. */
typedef struct {
    char  *p;
    size_t len, room;
    int    bad;
} Buf;

static void put(Buf *b, const char *s, size_t n)
{
    if (b->bad)
        return;
    if (b->len + n + 1 > b->room) {
        size_t want = (b->len + n + 1) * 2;
        char *bigger = realloc(b->p, want);

        if (!bigger) {
            b->bad = 1;
            return;
        }
        b->p = bigger;
        b->room = want;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

static void add(Buf *b, const char *s)
{
    put(b, s, strlen(s));
}

/* One phoneme string as one annotation. */
static void add_anno(Buf *b, const char *phones)
{
    add(b, "\x60[");
    add(b, phones);
    add(b, "] ");
}

/* A whole number as the words that say it, one annotation each.

   Urdu counts the Indian way, which is the whole of what is interesting
   here. Every number to ninety-nine has its own name -- forty-seven is
   saintaalees and is not built out of four and seven -- so the table is a
   hundred long rather than the thirty English would need. Above that the
   scale words are sau, hazaar, laakh, karor, arab: after a thousand it goes
   up by a hundred each time and not by a thousand, so 1234567 is twelve
   laakh, thirty-four thousand, five hundred, sixty-seven. Grouping it in
   threes would say something no Urdu speaker would. */
static void say_whole(Buf *b, unsigned long long n)
{
    static const struct { unsigned long long size; int word; } scale[] = {
        { 100000000000ULL, URDU_NUM_KHARAB },
        {   1000000000ULL, URDU_NUM_ARAB },
        {     10000000ULL, URDU_NUM_KAROR },
        {       100000ULL, URDU_NUM_LAKH },
        {         1000ULL, URDU_NUM_THOUSAND },
        {          100ULL, URDU_NUM_HUNDRED },
    };
    size_t i;

    if (n == 0) {
        add_anno(b, urdu_num_words[0]);
        return;
    }
    for (i = 0; i < sizeof scale / sizeof scale[0]; i++) {
        if (n >= scale[i].size) {
            /* How many of this scale there are, said as a number itself --
               twelve laakh, not one-two laakh -- so this recurses. */
            say_whole(b, n / scale[i].size);
            add_anno(b, urdu_num_words[scale[i].word]);
            n %= scale[i].size;
        }
    }
    if (n > 0)
        add_anno(b, urdu_num_words[n]);
}

/* Whether a run of code points is a number this can read, and if so, say it.
   After a decimal point the digits go one at a time, which is what every
   language this engine speaks does: three point one four is not three point
   a hundred and fourteen. */
static int say_number(Buf *b, const uint32_t *w, size_t n)
{
    unsigned long long whole = 0;
    size_t i = 0, point = n;
    int any = 0, neg = 0;

    if (n == 0)
        return 0;
    if (w[0] == '-') { neg = 1; i = 1; }
    for (; i < n; i++) {
        int d = digit_of(w[i]);

        if (d >= 0) {
            any = 1;
            if (point == n) {
                /* Past nineteen digits it is not a number anyone means to
                   have read as one, and it would overflow besides. */
                if (whole > 1000000000000000000ULL)
                    return 0;
                whole = whole * 10 + (unsigned)d;
            }
        } else if ((w[i] == '.' || w[i] == 0x066b) && point == n) {
            point = i;
        } else if (w[i] == ',' || w[i] == 0x066c) {
            continue;               /* a thousands separator, ignored */
        } else {
            return 0;
        }
    }
    if (!any)
        return 0;

    if (neg)
        add_anno(b, urdu_num_words[URDU_NUM_MINUS]);
    say_whole(b, whole);
    if (point < n) {
        add_anno(b, urdu_num_words[URDU_NUM_POINT]);
        for (i = point + 1; i < n; i++) {
            int d = digit_of(w[i]);

            if (d >= 0)
                add_anno(b, urdu_num_words[d]);
        }
    }
    return 1;
}

/* Keep the mark the sentence ended with. The engine reads a full stop as the
   end of a phrase and gives what came before it its own shape; without one a
   whole paragraph is a single breath. Urdu's full stop, comma and question
   mark are its own characters, none of which the engine knows, so each
   arrives as the western one it means. */
static void mark_end(Buf *b, const uint32_t *w, size_t n)
{
    size_t i = n;
    const char *mark = 0;

    while (i > 0 && (w[i - 1] == '"' || w[i - 1] == 0x27 || w[i - 1] == ')'))
        i--;
    if (i == 0)
        return;
    if (w[i - 1] == 0x06d4 || w[i - 1] == '.')
        mark = ".";
    else if (w[i - 1] == 0x061f)
        mark = "?";
    else if (w[i - 1] == 0x060c || w[i - 1] == ',')
        mark = ",";
    if (!mark)
        return;

    /* The mark belongs against the bracket it follows and not after the
       space that separates words, so whatever separator is already there
       comes off first. phones.py has no such trouble: it builds a list and
       joins it, where this appends as it goes. */
    while (b->len > 0 && b->p[b->len - 1] == ' ')
        b->p[--b->len] = 0;
    add(b, mark);
    add(b, " ");
}

/* One word: the lexicon, or a number, or the letters themselves.

   A word the lexicon has not got used to be dropped, and that was wrong. It
   was defended on the grounds that an annotation naming a phoneme the module
   does not have is spoken aloud, backticks and all -- which is true, and is
   an argument against guessing at phonemes, not an argument for silence. The
   lexicon covers 99.3% of ordinary Urdu by token, but the other 0.7% is
   where the names are, and a person's own name coming back as nothing at all
   is the worst thing this can do.

   So an unknown word goes through as its own letters. The engine reads plain
   text in annotation mode -- measured: a raw word between two annotations
   adds its own length to the utterance -- and the module has code points for
   every Urdu letter, so what comes out is the letter-by-letter reading the
   whole voice used to give. It has no short vowels in it and it is not
   right. It is a word said imperfectly instead of a word not said. */
static void say_word(Buf *b, const uint32_t *w, size_t n)
{
    char bare[512];
    char *p = bare;
    size_t i, first = 0, last = n;
    const char *said;

    if (n == 0)
        return;

    /* A number whole first, because the full stop is both a decimal point
       and the end of a sentence: 3.14 is tried before anything is stripped
       off it. */
    if (say_number(b, w, n))
        return;

    while (first < last && is_punct(w[first]))
        first++;
    while (last > first && is_punct(w[last - 1]))
        last--;
    if (first == last)
        return;

    if (say_number(b, w + first, last - first)) {
        mark_end(b, w, n);
        return;
    }

    for (i = first; i < last; i++) {
        uint32_t c = normalise(w[i]);

        if (c == 0)
            continue;
        if ((size_t)(p - bare) + 4 >= sizeof bare)
            return;                 /* longer than any Urdu word; not ours */
        encode(c, &p);
    }
    *p = 0;
    if (p == bare)
        return;

    said = look_up(bare);
    if (said) {
        add_anno(b, said);
        mark_end(b, w, n);
        return;
    }

    /* Not in the lexicon: the letters, as they were written. The
       normalisation above is for finding a word in a table and is not wanted
       here -- the module has its own code points for the look-alikes and for
       the vowel points, and it should see the spelling the writer used. A
       space after, so it cannot run into whatever follows. */
    {
        char raw[512];
        char *q = raw;

        for (i = first; i < last; i++) {
            if ((size_t)(q - raw) + 4 >= sizeof raw)
                return;
            encode(w[i], &q);
        }
        *q = 0;
        if (q == raw)
            return;
        add(b, raw);
        add(b, " ");
        mark_end(b, w, n);
    }
}

char *urdu_annotate(const char *utf8)
{
    Buf b;
    uint32_t *cp;
    size_t n = 0, i, start;
    size_t bytes = strlen(utf8);

    if (bytes == 0)
        return 0;

    /* Decoded once and worked on as code points. Splitting words and reading
       numbers both look backwards and forwards a character at a time, and
       doing that over UTF-8 in place is where this kind of code goes wrong. */
    cp = malloc((bytes + 1) * sizeof *cp);
    if (!cp)
        return 0;
    for (i = 0; i < bytes; ) {
        int len;

        cp[n++] = decode(utf8 + i, &len);
        i += (size_t)len;
    }

    b.p = 0; b.len = 0; b.room = 0; b.bad = 0;

    for (i = 0; i < n; ) {
        while (i < n && (cp[i] == ' ' || cp[i] == '\t' ||
                         cp[i] == '\n' || cp[i] == '\r'))
            i++;
        if (i >= n)
            break;
        start = i;
        while (i < n && cp[i] != ' ' && cp[i] != '\t' &&
               cp[i] != '\n' && cp[i] != '\r')
            i++;
        say_word(&b, cp + start, i - start);
    }
    free(cp);

    if (b.bad || b.len == 0) {
        free(b.p);
        return 0;
    }
    return b.p;
}
