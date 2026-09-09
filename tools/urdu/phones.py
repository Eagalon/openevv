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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numbers as urdu_numbers
import stress

# espeak's and Wiktionary's IPA -> the phoneme the module has for it. Two
# characters first, or the one-character rules eat their heads.
PAIRS = [
    (u"t͡ʃ", u"C"), (u"d͡ʒ", u"J"), (u"tʃ", u"C"), (u"dʒ", u"J"),
    (u"ɟʝ", u"J"), (u"cç", u"C"),
    # espeak writes ڑ as an r with a full stop after it -- its own notation
    # for a retroflex, leaking into what claims to be IPA. Four thousand eight
    # hundred words of the lexicon carry one, and every ڑ in them came out a
    # plain tap, because the r matched and the stop fell through. The locus
    # written for ڑ was only ever reached by WikiPron's words.
    (u"r.", u"R"),
    (u"aɪ", u"ay"), (u"aʊ", u"aw"),
    # The retroflexes, all three of which now have a sound of their own and
    # share one locus, urdu_retroflex_Fv: they differ in manner, not place.
    # R is the flap; the two stops are N and Z, Italian's gn and its ʒ, taken
    # over because no Urdu word wants either and there was nowhere else to
    # put a phoneme -- see is_val.up. That trade costs ژ, which is now said
    # with a ڈ; it is a dozen borrowed words and the dentals are everywhere.
    (u"ɽ", u"R"), (u"ʈ", u"N"), (u"ɖ", u"Z"), (u"ɳ", u"n"),
    (u"ʃ", u"S"), (u"ʒ", u"z"), (u"ŋ", u"G"),
    # ج, and the fault that cost most of anything measured today. espeak
    # writes it as a bare ɟ where this table had only ɟʝ, so it matched
    # nothing and was dropped outright: جلدی came out aldii. Weighted by how
    # often the words carrying it are said, that was 2.4% of all Urdu
    # speech with a consonant simply missing from it.
    (u"ɟ", u"J"),
    # espeak's retroflex sibilants, which Urdu does not distinguish from
    # the plain ones, and three symbols out of its English rules.
    (u"ʂ", u"S"), (u"ʐ", u"z"), (u"ð", u"d"), (u"ɒ", u"c"),
    # Urdu's ɪ and ʊ go to i and u, short, and not to e and o.
    #
    # They went to e and o for most of this branch's life, and that was a
    # workaround for a problem that no longer exists. Before vowel length was
    # written, a single i was the only i there was and Italian's is tense, so
    # دل came out deel; e was nearer. Length arrived and the workaround stayed,
    # and it was doing real damage: دل said del, ملک molk, دن den, تم tom
    # and اردو ordu -- the name of the language, said wrong, in every
    # sentence about it.
    #
    # Measured before changing it: dil is 750 ms against del's 763, and din
    # 740 against deen's 778. A single i is not longer than an e, so the
    # reason for the substitution was not there either. Now ɪ and iː differ
    # by length alone, which is exactly the contrast Urdu makes.
    # The lexicon writes a nasal vowel as one character rather than a vowel
    # and a combining tilde, so stripping the tilde never reaches these and
    # the whole vowel went missing: ہوں came out as an h and nothing
    # after it. The module has no nasal vowel, so they say the oral one.
    (u"ũ", u"u"), (u"ĩ", u"i"), (u"ã", u"a"), (u"õ", u"o"),
    (u"ẽ", u"e"), (u"ṽ", u"u"), (u"ẻ", u"e"),
    (u"ɪ", u"i"), (u"ʊ", u"u"), (u"ə", u"a"), (u"ʌ", u"a"),
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
    # خ is v, which is Italian's /v/ taken over for it: urdu_ph_x in
    # is_val.up speaks a velar fricative there now. It was a /k/ before,
    # which is a stop where Urdu has a fricative. غ, its voiced pair,
    # would want a second phoneme and there is not one: D and T are the
    # only ones left and neither is reachable from ital_con_vals.
    # ق stays a /k/ on purpose -- that is how most of Pakistan says it.
    (u"x", u"v"), (u"ɣ", u"g"), (u"q", u"k"), (u"h", u"L"), (u"ɦ", u"L"),
    (u"ʔ", u""),
]

# Aspiration, the dental and breathy diacritics, nasality: distinctions the
# module cannot make yet. Length is deliberately absent -- it is carried
# below, because Urdu's long vowels are half its vowels and dropping the mark
# made every one of them short.
DROP = u"\u0325\u032a\u0330\u031f\u0361\u02de\u02c8\u02cc\u1d4a\u032f\u02b7\u0295\u25cc"
LONG = u"\u02d0"
VOWELS = u"aeiouEcIUAY"

# The nasal vowels, written as one character by the lexicon and as a vowel
# and a combining tilde by espeak. The module has none, so a nasal vowel is
# the oral one and then an n -- which is what lang/plpl does with Polish's
# ogoneks, and what an Urdu speaker does anyway before a stop. میں was coming
# out mEE, with the nasality simply gone.
NASAL_ONE = u"ũĩãõẽṽẻ"
NASAL_MARK = u"̃"

# The same again for lang/enus, which is a better fit for Urdu than Italian
# in the places that matter most to an ear. It has /h/ as a phoneme of its
# own, so ہ needs no rule written for it. It has I and U -- the lax pair --
# where Italian has only the tense i and u, which is the substitution that
# made دن sound like den and اردو like ordu. And ga_ph_R already speaks at
# eng_ret_Fv, a retroflex locus IBM shipped, where Italian's had to be
# written. What it costs is English's own vowel reduction, which is in its
# rules rather than in its phonemes -- and phonemes are all this hands it,
# so the spelling never gets a vote.
#
# EVV_URDU_CHASSIS says which: enus for this, anything else for Italian.
PAIRS_EN = [
    (u"t͡ʃ", u"C"), (u"d͡ʒ", u"J"),
    (u"tʃ", u"C"), (u"dʒ", u"J"),
    (u"ɟʝ", u"J"), (u"cç", u"C"),
    (u"aɪ", u"Y"), (u"aʊ", u"W"),
    # ɽ takes the retroflex r English already has
    (u"r.", u"R"),
    (u"ɽ", u"R"), (u"ʈ", u"t"), (u"ɖ", u"d"), (u"ɳ", u"n"),
    (u"ʃ", u"S"), (u"ʒ", u"Z"), (u"ŋ", u"G"),
    (u"ɟ", u"J"), (u"ʂ", u"S"), (u"ʐ", u"z"),
    # the nasal vowels, written as one character by the lexicon. These were
    # in the Italian table and not this one, so ہوں came out as a bare h and
    # کیوں as ky -- the whole vowel gone, not just its nasality.
    (u"ũ", u"U"), (u"ĩ", u"I"), (u"ã", u"a"),
    (u"õ", u"o"), (u"ẽ", u"e"), (u"ṽ", u"U"),
    (u"ẻ", u"e"),
    # the lax pair, which is the whole reason this is worth trying
    (u"ɪ", u"I"), (u"ʊ", u"U"),
    (u"ə", u"a"), (u"ʌ", u"a"),
    (u"æ", u"A"), (u"ɛ", u"E"), (u"ɔ", u"c"),
    (u"ɑ", u"a"), (u"a", u"a"), (u"e", u"e"), (u"i", u"i"),
    (u"o", u"o"), (u"u", u"u"),
    (u"b", u"b"), (u"p", u"p"), (u"t", u"t"), (u"d", u"d"), (u"k", u"k"),
    (u"ɡ", u"g"), (u"g", u"g"), (u"f", u"f"), (u"v", u"v"), (u"s", u"s"),
    (u"z", u"z"), (u"m", u"m"), (u"n", u"n"), (u"l", u"l"), (u"r", u"F"),
    # Urdu's ر is a tap, and F is English's own -- the flap in the middle of
    # butter, at the alveolar locus, rather than the r of red which is a
    # retroflex approximant and belongs to ڑ.
    (u"ɾ", u"F"), (u"j", u"y"), (u"w", u"w"), (u"ʋ", u"w"),
    (u"c", u"C"),
    # /h/ is a phoneme here rather than a rule of ours
    (u"x", u"k"), (u"ɣ", u"g"), (u"q", u"k"),
    (u"h", u"h"), (u"ɦ", u"h"), (u"ʔ", u""),
]

# Aspiration, and the diacritic for a breathy voiced stop. Urdu has a
# four-way stop series where Italian has two, and the aspirated and
# breathy halves of it were being dropped outright: رکھتا came out rakta.
# Neither is a phoneme here, but /h/ is one now, and a stop followed by
# breath is what an aspirate is -- so the mark becomes an L after the
# consonant. The breathy pair are voiced through the breath rather than
# after it, which this cannot say; they get the same treatment, which is
# nearer than nothing and not right.
ASPIRATE = u"\u02b0\u0324\u02b1"

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LEXICON = os.path.join(ROOT, "references", "lexicons", "urd_arab_broad.tsv")
# What espeak says, written down once by tools/urdu/lexbuild.py so that
# nothing has to ask it again. It is read after WikiPron and before espeak
# itself: a transcription a person made beats a rule, and a rule written
# down beats one that needs a program on the machine to run it.
ESPEAK_LEXICON = os.path.join(ROOT, "references", "lexicons",
                              "urd_espeak.tsv")
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



CHASSIS = os.environ.get("EVV_URDU_CHASSIS", "itit")


def table():
    return PAIRS_EN if CHASSIS == "enus" else PAIRS


def doubles():
    """Whether a long vowel is written twice.

    Italian has one vowel where Urdu has two and no way to ask for a long
    one, so length there is the phoneme said again. English has the length
    already, in the tense and lax pairs -- i against I, u against U -- and
    doubling on top of that adds a whole second vowel: measured, a repeat
    costs 34 milliseconds in Italian and 95 in English, which is a word
    spoken with a stutter in the middle of it.
    """
    return CHASSIS != "enus"


def aspirate_of():
    """What breath after a stop is written as. English has /h/; Italian has
    the L this branch took over for it."""
    return u"h" if CHASSIS == "enus" else u"L"


def word(w):
    """One word of IPA as one pronunciation annotation."""
    w = u"".join(c for c in w if c not in DROP)
    out, i = [], 0
    while i < len(w):
        for src, dst in table():
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
                        dst = dst + aspirate_of()
                out.append(dst)
                break
        else:
            i += 1
    body = u"".join(out)
    # Where the stress falls is the word's own business rather than the
    # source's: espeak marks one and the lexicon marks none, and neither
    # knows Urdu's rule. tools/urdu/stress.py works it out of the shape
    # of the syllables, which is what Urdu stress actually follows.
    if not body:
        return u""
    # The stress rule reads weight, and weight is length, so the vowel has
    # to still be doubled when it runs. Only after the mark is placed can
    # the doubling come out again for a chassis that does not want it.
    marked = stress.mark(body)
    if not doubles():
        out, i = [], 0
        while i < len(marked):
            out.append(marked[i])
            if marked[i] in VOWELS:
                while i + 1 < len(marked) and marked[i + 1] == marked[i]:
                    i += 1
            i += 1
        marked = u"".join(out)
    return u"`[" + marked + u"]"


# Words the lexicon gets wrong for our purposes, and what they should be.
#
# Wiktionary transcribes a word the way it is often said rather than the way
# it is taught: ہے is given as a close eː where Urdu says an open ɛː, and it
# is the commonest word in the language, so it is worth saying properly. Each
# of these was heard and then changed, not assumed.
OVERRIDE = {
    u"ہے": u"ɦɛː",          # ہے, an open e
    u"ہیں": u"ɦɛː̃",  # ہیں, the same nasalised
    u"ہےں": u"ɦɛː̃",
}


def read_tsv(path, into):
    """A word and its IPA to a line. The first pronunciation wins.

    Wiktionary lists more than one for some words -- a formal reading and an
    everyday one, or two dialects -- and the first is taken, which is a
    choice rather than an answer. The same rule serves for reading a second
    file over the top of the first: what is already there stays.
    """
    try:
        f = io.open(path, encoding="utf-8")
    except IOError:
        return into
    for line in f:
        if line.startswith("#") or "\t" not in line:
            continue
        w, p = line.rstrip("\n").split("\t", 1)
        into.setdefault(w, p.replace(" ", ""))
    f.close()
    return into


def lexicon():
    """Every word anything in the tree knows how to say.

    Two files, in the order they are believed. WikiPron first, which is
    Wiktionary and so is people writing down what a word sounds like.
    espeak's second, which is its letter-to-sound rules and its own Urdu
    dictionary, run once by tools/urdu/lexbuild.py and written down: ten
    thousand words of it against WikiPron's six.

    Reading the second is what makes espeak optional at run time. It used to
    be asked for every word the first had not got, so a machine without
    espeak on it dropped those words out of the sentence and said nothing
    about having done so -- and a machine with espeak lost the lot whenever
    the batch did not come back word for word.
    """
    return read_tsv(ESPEAK_LEXICON, read_tsv(LEXICON, {}))


def mark_end(written, spoken):
    """Put back the mark the sentence ended with.

    The engine reads a full stop as the end of a phrase and gives what came
    before it its own shape; without one a whole paragraph is a single
    breath. Urdu's full stop is U+06D4 and its comma and question mark are
    its own too, none of which the engine knows, so each arrives as the
    western one it means.
    """
    if not spoken:
        return spoken
    tail = written.rstrip(u"\u0022\u0027)")
    if tail.endswith((u"۔", u".")):
        return spoken + u"."
    if tail.endswith(u"؟"):
        return spoken + u"?"
    if tail.endswith((u"،", u",")):
        return spoken + u","
    return spoken


def _ask(espeak, words):
    """One batch, or None if espeak did not answer word for word."""
    try:
        r = subprocess.run([espeak, "-v", "ur", "-q", "--ipa"],
                           input=u"\n".join(words), capture_output=True,
                           text=True, encoding="utf-8")
    except OSError:
        return None
    said = re.sub(r"\((en|ur)\)", u" ", r.stdout).split()
    return said if len(said) == len(words) else None


def from_espeak(words):
    """What espeak says, for the words neither lexicon has got.

    Asked in batches and, when a batch does not line up, one word at a time.
    It used to be one call for the whole list, its answer used only if
    exactly as many words came back as went in -- and espeak does not always
    oblige: it splits a word, joins two, or says nothing for one it cannot
    read. Three hundred words went in and nothing came back, and every one of
    them was then dropped from the sentence with nothing said about it.
    Falling back to one at a time costs a moment on the few words that need
    it and loses none of them.
    """
    if not words:
        return {}
    espeak = os.environ.get("ESPEAK", "espeak-ng")
    got, step = {}, 100
    for i in range(0, len(words), step):
        batch = words[i:i + step]
        said = _ask(espeak, batch)
        if said is None:
            for w in batch:
                one = _ask(espeak, [w])
                if one:
                    got[w] = one[0]
        else:
            got.update(zip(batch, said))
    return got


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

    # A line beginning with a hash is a note in the file rather than
    # something to say: the test texts are sectioned that way.
    keep = [L for L in text.split(u"\n")
            if not L.lstrip().startswith(u"#")]
    text = u"\n".join(keep)
    words = text.split()
    lex = lexicon()
    missing = [w for w in words if normalise(w.strip(PUNCT)) not in lex]
    guessed = from_espeak(missing)

    said = []
    for w in words:
        # A number first, because no lexicon can hold one and because the
        # full stop is both a decimal point and the end of a sentence: 3.14
        # is tried whole before anything is stripped off it, and 25۔ only
        # after. tools/urdu/numbers.py says why this is not left to espeak.
        num = urdu_numbers.ipa_of(w)
        if num is None:
            num = urdu_numbers.ipa_of(w.strip(PUNCT))
        if num is not None:
            # A number is several words -- twelve laakh, thirty-four
            # thousand -- and each gets its own annotation, so the engine
            # phrases them instead of running them into one long word.
            said.append(mark_end(w, u" ".join(
                x for x in (word(p) for p in num.split(u" ")) if x)))
            continue

        bare = normalise(w.strip(PUNCT))
        ipa = (OVERRIDE.get(bare) or lex.get(bare)
               or guessed.get(w) or guessed.get(bare))
        if not ipa:
            # Nothing knows this word, so it goes through as its own
            # letters. It used to be dropped, and that was wrong: the
            # argument for dropping was that an annotation naming a phoneme
            # the module has not got is spoken aloud, backticks and all --
            # which is an argument against guessing at phonemes, not an
            # argument for silence. The lexicon covers 99.3% of ordinary
            # Urdu by token and the rest is where the names are.
            #
            # The engine reads plain text in annotation mode, measured: a
            # raw word between two annotations adds its own length to the
            # utterance. So what comes out is the letter-by-letter reading,
            # with no short vowels in it and not right. A word said
            # imperfectly rather than a word not said.
            raw = w.strip(PUNCT)
            if raw:
                said.append(mark_end(w, raw))
            continue
        if ipa:
            # Wiktionary transcribes some words the way they are often said
            # rather than the way they are written: ہے is given as eː with no
            # h in it at all. A word beginning with ہ or ح is pronounced with
            # one, so it is put back rather than lost.
            if bare[:1] in (u"ہ", u"ح") and ipa[:1] not in (u"h", u"ɦ"):
                ipa = u"ɦ" + ipa
            said.append(mark_end(w, word(ipa)))
    print(u" ".join(x for x in said if x))
