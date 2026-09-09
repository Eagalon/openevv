#!/usr/bin/env bash
# The C lexicon and the Python one have to say the same thing.
#
# sapi/urdu_text.c is a second implementation of tools/urdu/phones.py,
# written because the first is Python and the voice a person installs cannot
# run it. Two implementations of one thing is how a thing comes to disagree
# with itself. This runs the same text through both and diffs.
#
# What it is not. It says nothing about whether the pronunciation is right --
# that is what the ear and dist/urdu-samples are for. It asks only whether
# the voice a person installs says what the WAVs say, which is a question a
# diff can answer and a listener cannot.
#
# The Python side is run with ESPEAK pointing at nothing on purpose: the C
# has no espeak and never will, so a fair comparison gives the Python none
# either. If this passes with espeak on the path and fails without it, the
# lexicon has a hole the C cannot see around.
#
# usage: test/urdu_agree.sh [case.txt ...]
set -uo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

bin=${EVV_URDU_TEXT:-build/urdu_text.exe}
[ -x "$bin" ] || bin=build/urdu_text
if [ ! -x "$bin" ]; then
    echo "urdu_agree: no harness at $bin -- make urdu-agree builds it" >&2
    exit 2
fi

cases=("$@")
if [ ${#cases[@]} -eq 0 ]; then
    mapfile -t cases < <(ls test/cases/urdu*.txt 2>/dev/null)
fi
if [ ${#cases[@]} -eq 0 ]; then
    echo "urdu_agree: no cases" >&2
    exit 2
fi

fail=0
for c in "${cases[@]}"; do
    ESPEAK=/nonexistent-on-purpose python3 tools/urdu/phones.py < "$c" \
        > build/agree-py.txt
    "$bin" < "$c" > build/agree-c.txt
    # --strip-trailing-cr because the harness is a Windows program and its
    # stdout is in text mode, so every line it writes gains a carriage
    # return the Python's does not have. That is the console's doing and not
    # a difference in what either says.
    if diff -q --strip-trailing-cr build/agree-py.txt build/agree-c.txt \
            > /dev/null; then
        printf 'agree   %s\n' "$c"
    else
        printf 'DIFFER  %s\n' "$c"
        diff --strip-trailing-cr build/agree-py.txt build/agree-c.txt | head -12
        fail=1
    fi
done

if [ $fail -ne 0 ]; then
    echo "urdu_agree: the voice does not say what the samples say" >&2
    exit 1
fi
echo "urdu_agree: ${#cases[@]} case(s), the two say the same thing"
