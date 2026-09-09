# -*- coding: utf-8 -*-
"""Where the stress goes in an Urdu word, out of the phonemes it is made of.

Urdu stress is not a fixed syllable the way Polish's is: it is weight, and the
weight is the shape of the syllable. A syllable is light if it is a short
vowel and nothing after it, heavy if the vowel is long or a consonant closes
it, and superheavy if both. The stress goes on the rightmost superheavy
syllable; failing that on the rightmost heavy one that is not the last; and
failing that on the first.

That is what makes کتاب ki-TAAB and پاکستان paakis-TAAN, where both end in a
superheavy syllable, and پانی PAA-ni, where neither does and the rule falls
back to the first of two heavy ones. Putting the mark at the front of every
word instead -- which is what this did before -- is most of what made the
result sound like somebody reading Urdu rather than speaking it.
"""

VOWELS = u"aeiouEc"


def syllables(ph):
    """The word split into (onset, nucleus, coda), one tuple a syllable."""
    # where the vowels are, with a doubled vowel counting once
    nuclei = []
    i = 0
    while i < len(ph):
        if ph[i] in VOWELS:
            j = i + 1
            while j < len(ph) and ph[j] == ph[i]:
                j += 1
            nuclei.append((i, j))
            i = j
        else:
            i += 1
    if not nuclei:
        return []

    out = []
    pos = 0
    for n, (a, b) in enumerate(nuclei):
        onset = ph[pos:a]
        after = nuclei[n + 1][0] if n + 1 < len(nuclei) else len(ph)
        run = ph[b:after]
        # the last consonant of a run belongs to the syllable after it, which
        # is the onset every language prefers; the rest close this one. The
        # position has to move past the coda and no further, or the coda is
        # counted again as the next onset -- which is what turned rakLtaa
        # into rakLkLtaa.
        coda = run if n + 1 == len(nuclei) else run[:-1] if run else u""
        out.append((onset, ph[a:b], coda))
        pos = b + len(coda)
    return out


def weight(nucleus, coda):
    long_v = len(nucleus) > 1
    closed = len(coda) > 0
    if long_v and closed:
        return 3
    if long_v or closed:
        return 2
    return 1


def stressed(ph):
    """Which syllable takes the stress, or None if the word has no vowel."""
    syl = syllables(ph)
    if not syl:
        return None
    w = [weight(n, c) for _o, n, c in syl]
    if len(syl) == 1:
        return 0
    for i in range(len(syl) - 1, -1, -1):
        if w[i] == 3:
            return i
    for i in range(len(syl) - 2, -1, -1):
        if w[i] == 2:
            return i
    return 0


def mark(ph):
    """The phonemes with .1 in front of the syllable that takes the stress."""
    syl = syllables(ph)
    at = stressed(ph)
    if at is None:
        return ph
    out = []
    for i, (o, n, c) in enumerate(syl):
        out.append((u".1" if i == at else u"") + o + n + c)
    # anything before the first onset, which a word starting with a vowel has
    used = sum(len(o) + len(n) + len(c) for o, n, c in syl)
    return u"".join(out) + (ph[used:] if used < len(ph) else u"")


if __name__ == "__main__":
    for w in (u"ketaab", u"paakestaan", u"paanii", u"salaam", u"del",
              u"mEEn", u"Luun", u"rakLtaa", u"laRkii", u"Sokreya",
              u"meeraa", u"naam", u"aLmad", u"booltaa"):
        print("%-12s -> %-14s syllables %s" % (
            w, mark(w),
            [o + n + c for o, n, c in syllables(w)]))
