# -*- coding: utf-8 -*-
"""Urdu text as phonemes the engine reads, out of a lexicon first.

Urdu does not write its short vowels, so nothing that reads the spelling can
know them: کتاب is k-t-a-b on the page and kɪtaːb in the mouth. Looking the
word up is the only thing that answers that, and it is what every description
of the problem says a real system does.

Two sources, in this order.

  references/lexicons/urd_arab_broad.tsv -- Urdu words with their IPA, mined
  from Wiktionary by WikiPron. It has the short vowels, it has length, and it
  knows words espeak does not: شکریہ comes back as ʃʊkɾɪjɑ where espeak spells
  that word out letter by letter.

  espeak-ng -v ur -q --ipa, for anything the lexicon has not got. Its Urdu is
  `status testing', which is why it is second rather than first.

What comes out is the engine's own phonemes inside the `[...]' annotation
docs/api.md describes, not letters. Writing letters and letting the module
read them is what turned دل into deel, because Italian reads an i long.

A long vowel is the same phoneme twice: the engine has no length and no way
to ask for one. Measured, a repeat is worth about forty milliseconds.

usage: tools/urdu/phones.py < urdu-text
       tools/urdu/phones.py --ipa < ipa       what espeak already answered
"""

import io
import os
import re
import subprocess
import sys

# espeak's and Wiktionary's IPA -> the phoneme the module has for it. Two
# characters first, or the one-character rules eat their heads.
PAIRS = [
    (u"t͡ʃ", u"C"), (u"d͡ʒ", u"J"), (u"tʃ", u"C"), (u"dʒ", u"J"),
    (u"ɟʝ", u"J"), (u"cç", u"C"),
    (u"aɪ", u"ay"), (u"aʊ", u"aw"),
    # the retroflexes. R is the flap and has a sound of its own; T and D are
    # declared retroflex and have none yet, so they say the dental for now.
    (u"ɽ", u"R"), (u"ʈ", u"t"), (u"ɖ", u"d"), (u"ɳ", u"n"),
    (u"ʃ", u"S"), (u"ʒ", u"Z"), (u"ŋ", u"G"),
    # Urdu's ɪ and ʊ are lax where Italian's i and u are not, so the lax pair
    # go to the open vowels: dil stays dil instead of stretching into deel.
    # The lexicon writes a nasal vowel as one character rather than a vowel
    # and a combining tilde, so stripping the tilde never reaches these and
    # the whole vowel went missing: ہوں came out as an h and nothing
    # after it. The module has no nasal vowel, so they say the oral one.
    (u"ũ", u"u"), (u"ĩ", u"i"), (u"ã", u"a"), (u"õ", u"o"),
    (u"ẽ", u"e"), (u"ṽ", u"u"), (u"ẻ", u"e"),
    (u"ɪ", u"e"), (u"ʊ", u"o"), (u"ə", u"a"), (u"ʌ", u"a"),
    (u"æ", u"E"), (u"ɛ", u"E"), (u"ɔ", u"c"),
    (u"ɑ", u"a"), (u"a", u"a"), (u"e", u"e"), (u"i", u"i"),
    (u"o", u"o"), (u"u", u"u"),
    (u"b", u"b"), (u"p", u"p"), (u"t", u"t"), (u"d", u"d"), (u"k", u"k"),
    (u"ɡ", u"g"), (u"g", u"g"), (u"f", u"f"), (u"v", u"v"), (u"s", u"s"),
    (u"z", u"z"), (u"m", u"m"), (u"n", u"n"), (u"l", u"l"), (u"r", u"r"),
    (u"ɾ", u"r"), (u"j", u"y"), (u"w", u"w"), (u"ʋ", u"w"), (u"c", u"C"),
    # ones the module has not got: the nearest there is, and /h/ nothing at
    # all, since h is not a phoneme this module declares and an annotation
    # naming one it does not know is spoken aloud rather than refused.
    # /h/ is L, which is Italian's gli taken over for it: urdu_ph_h in
    # is_val.up is what that code speaks now.
    (u"x", u"k"), (u"ɣ", u"g"), (u"q", u"k"), (u"h", u"L"), (u"ɦ", u"L"),
    (u"ʔ", u""),
]

# Aspiration, the dental and breathy diacritics, nasality: distinctions the
# module cannot make yet. Length is deliberately absent -- it is carried
# below, because Urdu's long vowels are half its vowels and dropping the mark
# made every one of them short.
DROP = u"\u0325\u032a\u0330\u031f\u0361\u02de\u02c8\u02cc"
LONG = u"\u02d0"
VOWELS = u"aeiouEc"

# The nasal vowels, written as one character by the lexicon and as a vowel
# and a combining tilde by espeak. The module has none, so a nasal vowel is
# the oral one and then an n -- which is what lang/plpl does with Polish's
# ogoneks, and what an Urdu speaker does anyway before a stop. میں was coming
# out mEE, with the nasality simply gone.
NASAL_ONE = u"ũĩãõẽṽẻ"
NASAL_MARK = u"̃"

# Aspiration, and the diacritic for a breathy voiced stop. Urdu has a
# four-way stop series where Italian has two, and the aspirated and
# breathy halves of it were being dropped outright: رکھتا came out rakta.
# Neither is a phoneme here, but /h/ is one now, and a stop followed by
# breath is what an aspirate is -- so the mark becomes an L after the
# consonant. The breathy pair are voiced through the breath rather than
# after it, which this cannot say; they get the same treatment, which is
# nearer than nothing and not right.
ASPIRATE = u"\u02b0\u0324"

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LEXICON = os.path.join(ROOT, "references", "lexicons", "urd_arab_broad.tsv")
PUNCT = u"\u060c\u061f\u06d4.,!?;:\"'()"

# Urdu letters that arrive as their Arabic or Persian look-alikes, because
# that is what an Arabic keyboard gives and what much text on the web
# carries. The lexicon is spelled the Urdu way, so a word written the other
# way misses it and falls through to espeak for no reason.
SAME = {
    u"\u0643": u"\u06a9",   # kaf -> keheh
    u"\u064a": u"\u06cc",   # yeh -> farsi yeh
    u"\u0649": u"\u06cc",   # alef maksura -> farsi yeh
    u"\u0647": u"\u06c1",   # heh -> heh goal
    u"\u0629": u"\u06c1",   # teh marbuta -> heh goal
    u"\u06c3": u"\u06c1",
}
# the vowel points, shadda, sukun and the kashida, none of which the
# lexicon spells and all of which stop a word matching it
for _c in (u"\u064e", u"\u064f", u"\u0650", u"\u0651", u"\u0652", u"\u0640"):
    SAME[_c] = u""


def normalise(w):
    return u"".join(SAME.get(c, c) for c in w)



def word(w):
    """One word of IPA as one pronunciation annotation."""
    stress = ".1" if u"\u02c8" in w else ""
    w = u"".join(c for c in w if c not in DROP)
    out, i = [], 0
    while i < len(w):
        for src, dst in PAIRS:
            if w.startswith(src, i):
                nasal = src in NASAL_ONE
                i += len(src)
                # a combining tilde after it says the same thing
                while i < len(w) and w[i] == NASAL_MARK:
                    nasal = True
                    i += 1
                if i < len(w) and w[i] == LONG:
                    i += 1
                    if dst and dst[-1] in VOWELS:
                        dst = dst + dst[-1]
                while i < len(w) and w[i] == NASAL_MARK:
                    nasal = True
                    i += 1
                if nasal and dst and dst[-1] in VOWELS:
                    dst = dst + u"n"
                # a stop with breath after it is what an aspirate is
                while i < len(w) and w[i] in ASPIRATE:
                    i += 1
                    if dst and dst[-1] not in VOWELS:
                        dst = dst + u"L"
                out.append(dst)
                break
        else:
            i += 1
    body = u"".join(out)
    return u"`[" + stress + body + u"]" if body else u""


def lexicon():
    """Every word the lexicon knows, and the first pronunciation it gives.

    Wiktionary lists more than one for some words -- a formal reading and an
    everyday one, or two dialects -- and the first is taken, which is a
    choice rather than an answer.
    """
    out = {}
    try:
        f = io.open(LEXICON, encoding="utf-8")
    except IOError:
        return out
    for line in f:
        if "\t" not in line:
            continue
        w, p = line.rstrip("\n").split("\t", 1)
        out.setdefault(w, p.replace(" ", ""))
    return out


def from_espeak(words):
    """What espeak says, for the words the lexicon has not got."""
    if not words:
        return {}
    espeak = os.environ.get("ESPEAK", "espeak-ng")
    try:
        r = subprocess.run([espeak, "-v", "ur", "-q", "--ipa"],
                           input=u" ".join(words), capture_output=True,
                           text=True, encoding="utf-8")
    except OSError:
        return {}
    said = re.sub(r"\((en|ur)\)", " ", r.stdout).split()
    return dict(zip(words, said)) if len(said) == len(words) else {}


if __name__ == "__main__":
    # Windows writes a carriage return with every newline, and one inside an
    # annotation stops the engine recognising it -- the sentence is then
    # spoken as its own backticks and comes out ten times too long.
    try:
        sys.stdout.reconfigure(newline="\n")
    except AttributeError:
        pass

    text = sys.stdin.read()
    if "--ipa" in sys.argv[1:]:
        text = re.sub(r"\((en|ur)\)", " ", text)
        print(u" ".join(x for x in (word(w) for w in text.split()) if x))
        sys.exit(0)

    words = text.split()
    lex = lexicon()
    missing = [w for w in words if normalise(w.strip(PUNCT)) not in lex]
    guessed = from_espeak(missing)

    said = []
    for w in words:
        bare = normalise(w.strip(PUNCT))
        ipa = lex.get(bare) or guessed.get(w) or guessed.get(bare)
        if ipa:
            # Wiktionary transcribes some words the way they are often said
            # rather than the way they are written: ہے is given as eː with no
            # h in it at all. A word beginning with ہ or ح is pronounced with
            # one, so it is put back rather than lost.
            if bare[:1] in (u"ہ", u"ح") and ipa[:1] not in (u"h", u"ɦ"):
                ipa = u"ɦ" + ipa
            said.append(word(ipa))
    print(u" ".join(x for x in said if x))
