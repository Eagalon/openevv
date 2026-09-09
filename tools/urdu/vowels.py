# -*- coding: utf-8 -*-
"""espeak's Urdu phonemes, written as letters lang/urpk already says.

Urdu does not write its short vowels, so the module reads کتاب as ktab. espeak
knows the vowels -- it answers kɪtˈaːb -- so this takes its answer and spells
it back out in letters the module pronounces, which needs no change to the
engine at all. Sounds the module has not got yet fall back to the nearest it
has, exactly as the codepoints table does.

The five that have no Latin letter in the module are written as the Urdu
letter instead, since the codepoints table already sends that to the right
byte and the result stays valid UTF-8.
"""
import re, sys

PAIRS = [
    # two characters first, or the one-character rules eat their heads
    (u"t͡ʃ", u"چ"), (u"d͡ʒ", u"ج"), (u"tʃ", u"چ"), (u"dʒ", u"ج"),
    (u"ɟʝ", u"ج"), (u"cç", u"چ"), (u"aɪ", u"ai"), (u"aʊ", u"au"),
    (u"ɑː", u"a"), (u"iː", u"i"), (u"uː", u"u"), (u"eː", u"e"), (u"oː", u"o"),
    (u"ɛː", u"e"), (u"ɔː", u"o"),
    # the retroflexes, which have bytes of their own
    (u"ʈ", u"ٹ"), (u"ɖ", u"ڈ"), (u"ɽ", u"ڑ"), (u"ɳ", u"ن"),
    (u"ʃ", u"ش"), (u"ʒ", u"ژ"), (u"ŋ", u"ں"),
    # vowels
    (u"ɑ", u"a"), (u"ʌ", u"a"), (u"ə", u"a"), (u"æ", u"e"), (u"ɛ", u"e"),
    (u"ɪ", u"i"), (u"ʊ", u"u"), (u"ɔ", u"o"), (u"o", u"o"), (u"e", u"e"),
    (u"i", u"i"), (u"u", u"u"), (u"a", u"a"),
    # consonants the module has
    (u"b", u"b"), (u"p", u"p"), (u"t", u"t"), (u"d", u"d"), (u"k", u"k"),
    (u"ɡ", u"g"), (u"g", u"g"), (u"f", u"f"), (u"v", u"v"), (u"s", u"s"),
    (u"z", u"z"), (u"m", u"m"), (u"n", u"n"), (u"l", u"l"), (u"r", u"r"),
    (u"j", u"y"), (u"w", u"w"), (u"ʋ", u"w"), (u"c", u"چ"),
    # ones it has not: the nearest there is, and h says nothing at all
    (u"x", u"k"), (u"ɣ", u"g"), (u"q", u"k"), (u"h", u"h"), (u"ɦ", u"h"),
    (u"ʔ", u""),
]

# stress, length, and the diacritics for dental, aspirated, breathy and nasal:
# every one of them a distinction the module cannot make yet.
DROP = u"ˈˌːʰ\u0325\u032a\u0324\u0303\u0330\u031f\u0361\u02de"

def one(word):
    word = u"".join(c for c in word if c not in DROP)
    out, i = [], 0
    while i < len(word):
        for src, dst in PAIRS:
            if word.startswith(src, i):
                out.append(dst); i += len(src); break
        else:
            out.append(word[i] if word[i].isalpha() else u" "); i += 1
    return u"".join(out)

if __name__ == "__main__":
    text = sys.stdin.read()
    # espeak marks a word it fell back to another language for as (en)...(ur)
    text = re.sub(r"\((en|ur)\)", " ", text)
    print(u" ".join(one(w) for w in text.split()))
