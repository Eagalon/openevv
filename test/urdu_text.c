/* Does the C lexicon say what the Python one says?

   sapi/urdu_text.c is a second implementation of what tools/urdu/phones.py
   does, written because the first one is Python and the voice a person
   installs cannot run it. Two implementations of one thing is how a thing
   comes to disagree with itself, so this exists to hold them together: it
   reads Urdu on standard input and writes what the C would hand the engine,
   and test/urdu_agree.sh runs the same text through both and diffs.

   It is deliberately not a test of whether the pronunciation is any good.
   That is what the ear is for. This asks only whether the two agree, which
   is the question a diff can answer and a listener cannot.

   usage: build/urdu_text < urdu-text */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "urdu_lex.h"

int main(void)
{
    char *text = NULL;
    size_t len = 0, room = 0;
    char *said;
    int c;

    while ((c = getchar()) != EOF) {
        if (len + 2 > room) {
            room = room ? room * 2 : 4096;
            text = realloc(text, room);
            if (!text)
                return 1;
        }
        text[len++] = (char)c;
    }
    if (!text)
        return 0;
    text[len] = 0;

    said = urdu_annotate(text);
    if (said) {
        /* One trailing space is how each word is joined; phones.py joins
           with one between and none after, so the comparison is on the
           words rather than on the whitespace. */
        size_t n = strlen(said);

        while (n > 0 && said[n - 1] == ' ')
            said[--n] = 0;
        printf("%s\n", said);
        free(said);
    } else {
        printf("\n");
    }
    free(text);
    return 0;
}
