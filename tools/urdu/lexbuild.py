#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Write down what espeak says, once, so that nothing has to ask it again.

tools/urdu/phones.py looks a word up in WikiPron's Urdu lexicon and asks
espeak for whatever is not there. That worked and had two faults worth
fixing rather than living with.

The first is that espeak has to be installed. It is a build-time dependency
of a thing that is meant to speak on a machine that has only this engine on
it, and a missing espeak is silent: `from_espeak' returns nothing and every
word it would have answered is dropped out of the sentence without a word
said about it.

The second is that the ask was all or nothing. espeak is handed the whole
list and the answer is only used if it comes back with exactly as many words
as went in, which it does not always do -- it splits a word, or joins two,
or says nothing for one it cannot read. Three hundred words went in and
nothing came back, and the caller could not tell that from espeak being
absent. So this asks in small batches and, when a batch does not line up,
one word at a time, which is slow once and correct.

What comes out is a TSV of the same shape as WikiPron's, in
references/lexicons, so both are read the same way and either can be looked
at by eye.

The vocabulary is the union of three things already in the tree: the
headwords of espeak's own ur_list, which is a hand-made Urdu dictionary of
some three thousand entries; a frequency list off Urdu subtitles, which is
what people actually say rather than what is written down; and whatever
words the test cases use.

usage: tools/urdu/lexbuild.py [out.tsv]
       ESPEAK=... names the binary.
"""

import io
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "references", "lexicons", "urd_espeak.tsv")

UR_LIST = os.path.join(ROOT, "references", "espeak-ng", "dictsource", "ur_list")
FREQ = os.path.join(ROOT, "references", "wordlists", "ur_full.txt")
CASES = os.path.join(ROOT, "test", "cases")

URDU = re.compile(u"^[\u0600-\u06ff\u0750-\u077f]+$")


def vocabulary():
    """Every Urdu word the tree has, from wherever it has one."""
    words = set()

    # espeak's own dictionary. A line is a word, a tab and espeak's phonemes;
    # the phonemes are in espeak's alphabet and not IPA, so only the headword
    # is taken here and espeak itself is asked what it sounds like.
    try:
        for line in io.open(UR_LIST, encoding="utf-8"):
            if line.startswith("//") or not line.strip():
                continue
            w = line.split()[0]
            if URDU.match(w):
                words.add(w)
    except IOError:
        pass

    # What people say, most often first. The count is dropped: this is a
    # vocabulary and not a language model.
    try:
        for line in io.open(FREQ, encoding="utf-8"):
            w = line.split()[0] if line.split() else ""
            if URDU.match(w):
                words.add(w)
    except IOError:
        pass

    # And the test cases, so that a word written to be listened to is never
    # the one word that has to go to espeak at run time.
    for name in os.listdir(CASES) if os.path.isdir(CASES) else []:
        if "urdu" not in name:
            continue
        for line in io.open(os.path.join(CASES, name), encoding="utf-8"):
            if line.lstrip().startswith("#"):
                continue
            for w in line.split():
                w = w.strip(u"\u060c\u061f\u06d4.,!?;:\"'()")
                if URDU.match(w):
                    words.add(w)

    return sorted(words)


def ask(espeak, words):
    """One batch. None if espeak did not answer word for word."""
    try:
        r = subprocess.run([espeak, "-v", "ur", "-q", "--ipa"],
                           input=u"\n".join(words), capture_output=True,
                           text=True, encoding="utf-8")
    except OSError:
        return None
    said = re.sub(r"\((en|ur)\)", u" ", r.stdout).split()
    return said if len(said) == len(words) else None


def main(argv):
    out = argv[0] if argv else OUT
    espeak = os.environ.get("ESPEAK", "espeak-ng")

    words = vocabulary()
    if not words:
        print("urdu/lexbuild: no vocabulary found", file=sys.stderr)
        return 1
    print("urdu/lexbuild: %d words" % len(words), file=sys.stderr)

    got, alone = {}, 0
    step = 200
    for i in range(0, len(words), step):
        batch = words[i:i + step]
        said = ask(espeak, batch)
        if said is None:
            # The batch did not line up, so each word is asked on its own.
            # This is the slow path and it is where the words espeak cannot
            # read get found rather than quietly taking a neighbour's answer.
            for w in batch:
                one = ask(espeak, [w])
                if one:
                    got[w] = one[0]
                alone += 1
        else:
            got.update(zip(batch, said))
        if (i // step) % 10 == 0:
            print("  %d/%d" % (i, len(words)), file=sys.stderr)

    if not got:
        print("urdu/lexbuild: espeak said nothing -- is it installed? "
              "ESPEAK=%s" % espeak, file=sys.stderr)
        return 1

    with io.open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(u"# Urdu word -> IPA, as espeak-ng says it.\n")
        f.write(u"# Written by tools/urdu/lexbuild.py; do not edit by hand.\n")
        f.write(u"# %d words, of which %d had to be asked one at a time.\n"
                % (len(got), alone))
        for w in sorted(got):
            f.write(u"%s\t%s\n" % (w, got[w]))
    print("urdu/lexbuild: wrote %d to %s (%d asked alone)"
          % (len(got), os.path.relpath(out, ROOT), alone), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
