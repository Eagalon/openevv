#!/usr/bin/env bash
# Speak Urdu with its short vowels in, by asking espeak what they are.
#
# Urdu does not write them, so the module reads کتاب as ktab. espeak's Urdu
# knows the word and answers kɪtˈaːb; tools/urdu/phones.py writes that back
# out as letters lang/urpk already says, and the engine speaks those. Nothing
# in the engine is changed and nothing of espeak's is linked -- it is asked,
# as a separate program, and only its answer is used.
#
# usage: tools/urdu/say.sh out.wav "اردو متن"
#        tools/urdu/say.sh out.wav -f file.txt
#
# ESPEAK says where espeak-ng is, EVV which binary to speak with, and
# EVV_URDU_LANG the language number lang/urpk was built as.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
espeak=${ESPEAK:-espeak-ng}
evv=${EVV:-$here/build/evv}
lang=${EVV_URDU_LANG:-0x120000}

out=$1; shift
if [ "${1:-}" = "-f" ]; then text=$(cat "$2"); else text="$*"; fi

command -v "$espeak" >/dev/null 2>&1 || {
    echo "urdu/say: no espeak-ng on the path; ESPEAK=... says where it is" >&2
    exit 2
}
[ -x "$evv" ] || { echo "urdu/say: no engine at $evv" >&2; exit 2; }

spelled=$(printf '%s\n' "$text" \
          | "$espeak" -v ur -q --ipa 2>/dev/null \
          | python3 "$here/tools/urdu/phones.py")

printf '%s\n' "$spelled" | "$evv" -L "$lang" -o "$out"
echo "urdu/say: $spelled"
