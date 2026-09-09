# -*- coding: utf-8 -*-
"""espeak's Urdu phonemes, as phonemes the engine reads -- not as letters.

The first bridge wrote espeak's answer out as Latin letters and let the module
read them, which meant Italian's spelling rules got a vote on every word: دل
came back as `deel', because Italian reads an i long. This writes the engine's
own phoneme alphabet instead, inside the `[...]' annotation docs/api.md
describes, so nothing is spelled and nothing is read.

Annotations have to be on for it: eciInputType is 1, which is `a' in cli/probe
and a setParam in a caller. With them off the annotation is spoken aloud
instead, which is what happens if this is piped into build/evv.

Phonemes are written one after another with no spaces between them, the way
eciGeneratePhonemes writes them: `[.2hE.1lo]' is how the engine says hello.
"""
import re, sys

# espeak's IPA -> the phoneme the module has for it. Two characters first, or
# the one-character rules eat their heads.
PAIRS = [
    (u"t͡ʃ", u"C"), (u"d͡ʒ", u"J"), (u"tʃ", u"C"), (u"dʒ", u"J"),
    (u"ɟʝ", u"J"), (u"cç", u"C"),
    (u"aɪ", u"ay"), (u"aʊ", u"aw"),
    (u"ɑː", u"a"), (u"iː", u"i"), (u"uː", u"u"), (u"eː", u"e"), (u"oː", u"o"),
    (u"ɛː", u"E"), (u"ɔː", u"c"),
    # the retroflexes. R is the flap and has a sound of its own; T and D are
    # declared retroflex and have none yet, so they say the dental until they
    # do.
    (u"ɽ", u"R"), (u"ʈ", u"t"), (u"ɖ", u"d"), (u"ɳ", u"n"),
    (u"ʃ", u"S"), (u"ʒ", u"Z"), (u"ŋ", u"G"),
    # the short vowels, which are the whole point. Urdu's ɪ and ʊ are lax and
    # Italian's i and u are not, so the lax pair go to the open vowels: dil
    # stays dil instead of stretching into deel.
    (u"ɪ", u"e"), (u"ʊ", u"o"), (u"ə", u"a"), (u"ʌ", u"a"),
    (u"æ", u"E"), (u"ɛ", u"E"), (u"ɔ", u"c"),
    (u"ɑ", u"a"), (u"a", u"a"), (u"e", u"e"), (u"i", u"i"),
    (u"o", u"o"), (u"u", u"u"),
    (u"b", u"b"), (u"p", u"p"), (u"t", u"t"), (u"d", u"d"), (u"k", u"k"),
    (u"ɡ", u"g"), (u"g", u"g"), (u"f", u"f"), (u"v", u"v"), (u"s", u"s"),
    (u"z", u"z"), (u"m", u"m"), (u"n", u"n"), (u"l", u"l"), (u"r", u"r"),
    (u"j", u"y"), (u"w", u"w"), (u"ʋ", u"w"), (u"c", u"C"),
    # ones the module has not got: the nearest there is, and /h/ nothing.
    (u"x", u"k"), (u"ɣ", u"g"), (u"q", u"k"), (u"h", u""), (u"ɦ", u""),
    (u"ʔ", u""),
]

# length, the dental and breathy diacritics, aspiration and nasality: every
# one a distinction the module cannot make yet.
DROP = u"ːʰ\u0325\u032a\u0324\u0303\u0330\u031f\u0361\u02de"

def word(w):
    stress = ".1" if u"ˈ" in w else ""
    w = u"".join(c for c in w if c not in DROP and c not in u"ˈˌ")
    out, i = [], 0
    while i < len(w):
        for src, dst in PAIRS:
            if w.startswith(src, i):
                out.append(dst); i += len(src); break
        else:
            i += 1
    body = u"".join(out)
    return u"`[%s%s]" % (stress, body) if body else u""

if __name__ == "__main__":
    text = re.sub(r"\((en|ur)\)", " ", sys.stdin.read())
    print(u" ".join(x for x in (word(w) for w in text.split()) if x))
