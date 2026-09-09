#!/usr/bin/env bash
# Speak Urdu, with the short vowels the writing does not give.
#
# tools/urdu/phones.py looks each word up -- WikiPron's Urdu lexicon first,
# espeak for what it has not got -- and writes the answer as the engine's own
# phonemes inside `[...]'. The engine speaks those. No letters anywhere in it:
# writing letters and letting the module read them is what turned دل into
# deel, because Italian reads an i long.
#
# -a is not optional. The engine ignores an annotation without it and speaks
# the backticks aloud instead, which comes out about ten times too long.
#
# usage: tools/urdu/say.sh out.wav "اردو متن"
#        tools/urdu/say.sh out.wav -f file.txt
#
# ESPEAK says where espeak-ng is, EVV which binary to speak with, and
# EVV_URDU_LANG the number lang/urpk was built as.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
evv=${EVV:-$here/build/evv}
lang=${EVV_URDU_LANG:-0x120000}

out=$1; shift
if [ "${1:-}" = "-f" ]; then text=$(cat "$2"); else text="$*"; fi

[ -x "$evv" ] || { echo "urdu/say: no engine at $evv" >&2; exit 2; }

said=$(printf '%s\n' "$text" | python3 "$here/tools/urdu/phones.py")
[ -n "$said" ] || { echo "urdu/say: nothing to say" >&2; exit 1; }

printf '%s\n' "$said" | "$evv" -a -L "$lang" -o "$out"
echo "urdu/say: $said"
