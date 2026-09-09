#!/usr/bin/env python3
# -*- coding: utf-8 -*-
u"""Read a number aloud in Urdu.

A lexicon cannot hold numbers -- there are infinitely many of them -- so
they are the one thing that has to be worked out rather than looked up.
Before this they were handed to espeak, which had two faults. espeak had to
be on the machine, and without it a number was dropped out of the sentence
in silence: "میرے پاس 25 کتابیں" came back with no twenty-five in it and
nothing said. And espeak returns nothing at all for a number written in
Urdu's own numerals, so ۱۲۳ was silent whether espeak was there or not.

Urdu counts the Indian way and that is the whole of what is interesting
here.

Every number from nought to ninety-nine has its own name. That is not how
English works -- twenty-one is two words there -- and it is why the table
this reads is a hundred entries long rather than the thirty English would
need. They are irregular besides: forty-seven is sɛŋtaːliːs, which is not
built out of four and seven in any way a rule could find.

Above a hundred the scale words are Indian: sau a hundred, hazaar a
thousand, laakh a hundred thousand, karor a hundred laakh, arab a hundred
karor. So the grouping is not the Western three digits at a time. 1,234,567
is baarah laakh, chauntees hazaar, paanch sau, sarsath -- twelve laakh and
thirty-four thousand and five hundred and sixty-seven -- and a program that
grouped it in thousands would say something no Urdu speaker would.

Three sets of digits arrive: ASCII, Urdu's own (U+06F0), and Arabic's
(U+0660), which look the same in most fonts and are different characters.
All three are read.

usage: from numbers import ipa_of; ipa_of(u"۱۲۳") -> IPA or None
"""

import io
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TABLE = os.path.join(ROOT, "references", "lexicons", "urd_numbers.tsv")

# The three ways a digit is written. Urdu text mixes them freely -- a date in
# Urdu numerals in a sentence with a price in ASCII ones is ordinary.
DIGITS = {}
for _base in (0x0030, 0x06f0, 0x0660):
    for _d in range(10):
        DIGITS[unichr(_base + _d) if False else chr(_base + _d)] = _d

HUNDRED = u"\u0633\u0648"
THOUSAND = u"\u06c1\u0632\u0627\u0631"
LAKH = u"\u0644\u0627\u06a9\u06be"
KAROR = u"\u06a9\u0631\u0648\u0691"
ARAB = u"\u0627\u0631\u0628"
KHARAB = u"\u06a9\u06be\u0631\u0628"
MINUS = u"\u0645\u0646\u0641\u06cc"
POINT = u"\u0627\u0639\u0634\u0627\u0631\u06cc\u06c1"

# Largest first, which is the order they are said in. The Indian scale: after
# a thousand it goes up by a hundred each time, not a thousand.
SCALES = ((10 ** 11, KHARAB), (10 ** 9, ARAB), (10 ** 7, KAROR),
          (10 ** 5, LAKH), (10 ** 3, THOUSAND), (100, HUNDRED))

_table = None


def table():
    """The hundred names and the scale words, read once."""
    global _table
    if _table is not None:
        return _table
    _table = {}
    try:
        f = io.open(TABLE, encoding="utf-8")
    except IOError:
        return _table
    for line in f:
        if line.startswith("#") or "\t" not in line:
            continue
        k, v = line.rstrip("\n").split("\t", 1)
        _table[k] = v
    f.close()
    return _table


def is_number(w):
    """Whether a word is one this can read: digits, and at most one point."""
    if not w:
        return False
    seen_point = False
    body = w[1:] if w[:1] == u"-" else w
    if not body:
        return False
    for c in body:
        if c in DIGITS:
            continue
        if c in u".\u066b" and not seen_point:   # the Arabic decimal separator too
            seen_point = True
            continue
        if c in u",\u066c":                      # a thousands separator, ignored
            continue
        return False
    return any(c in DIGITS for c in body)


def value(s):
    """The digits of a string as an integer, whichever set they are from."""
    return int(u"".join(str(DIGITS[c]) for c in s if c in DIGITS) or u"0")


def words(n):
    """A whole number as the sequence of table keys that says it.

    Nought is a special case: it is a word on its own and never appears
    inside a longer number, because nothing is ever said as "two hundred and
    nought and five".
    """
    if n == 0:
        return [u"0"]
    out = []
    for size, name in SCALES:
        if n >= size:
            # How many of this scale there are, said as a number itself --
            # twelve laakh, not one-two laakh -- so this recurses.
            out += words(n // size) + [name]
            n %= size
    if n:
        out.append(str(n))
    return out


def spoken(w):
    """The table keys for one written number, sign and decimal point and all.

    After a decimal point the digits are read one at a time, which is what
    Urdu does and what every language this engine speaks does: three point
    one four is not three point a hundred and fourteen.
    """
    neg = w[:1] == u"-"
    body = w[1:] if neg else w
    body = body.replace(u",", u"").replace(u"\u066c", u"")
    point = None
    for sep in (u".", u"\u066b"):
        if sep in body:
            point = body.index(sep)
            break
    whole = body if point is None else body[:point]
    frac = u"" if point is None else body[point + 1:]

    out = [MINUS] if neg else []
    out += words(value(whole))
    if frac:
        out.append(POINT)
        out += [str(DIGITS[c]) for c in frac if c in DIGITS]
    return out


def ipa_of(w):
    """One written number as one run of IPA, or None if it is not a number.

    The words are joined without a space because that is what the caller
    wants: tools/urdu/phones.py turns a run of IPA into one annotation, and
    an annotation is one word to the engine. A number said as several words
    would take several annotations and lose the phrase.
    """
    if not is_number(w):
        return None
    t = table()
    said = [t.get(k) for k in spoken(w)]
    if not all(said):
        return None
    return u" ".join(said)


if __name__ == "__main__":
    import sys
    for arg in sys.argv[1:]:
        print(u"%s\t%s" % (arg, ipa_of(arg)))
