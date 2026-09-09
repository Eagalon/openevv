# Third-party synthesisers, for reading

Cloned by hand, shallow, and ignored by git. Nothing here is built, linked,
or copied from. The tree is MIT; everything below is GPL or LGPL, so what
crosses out of this directory is understanding, never source. Formant
frequencies and bandwidths for a phoneme are measurements of a vocal tract
and are in the published literature -- Klatt 1980 above all -- so take them
from there and cite there, not from the tables in these repositories.

| Repo | Licence | Why it is here |
|---|---|---|
| espeak-ng | GPLv3 | Urdu and Hindi phoneme source, and the Urdu letter-to-sound rules |
| NVSpeechPlayer | GPLv2 | A Klatt frame table per phoneme, in readable form. The closest model to what openevv needs |
| rsynth | LGPLv2+ | Holmes element table: formant targets *and* the transition ranks between them |
| SAM | permissive-ish, unclear | A very small formant table; useful only as a sanity check on how little will do |
| RHVoice | LGPLv2.1 | Pipeline architecture only. No Urdu, and HTS-based rather than formant -- see note below |

## What is actually useful, file by file

### espeak-ng -- the Urdu inventory
- `phsource/ph_urdu` (45 lines) -- thin; inherits the rest from Hindi.
- `phsource/ph_hindi_base` (571 lines) -- the real inventory. Everything Urdu
  needs is declared here: aspirated stops as a `#` suffix (`p# b# t# d# c# J#
  k# g#`), retroflexes as a `.` suffix (`t. d.`, and aspirated `t.# d.#`), and
  the ten nasalised vowels (`i~ I~ e~ E~ a~ V~ O~ o~ U~ u~`).
- `dictsource/ur_rules` (31 KB) and `ur_list` (61 KB) -- the letter-to-sound
  source the installed `ur_dict` was compiled from. This is what to fix when
  the frontend gets a word wrong.
- `phsource/vwl_hi/`, `phsource/vowel/` -- the spectral data, but as compiled
  binary `SPECTSEQ` blobs, and for espeak's own resonance model. Not
  transferable to a Klatt frame.
- `phsource/klatt/` -- espeak's own Klatt tables, including `bh`, a
  breathy-voiced b. Directly relevant to Urdu's bʰ dʰ ɡʰ.

### NVSpeechPlayer -- the model to copy the *shape* of
`data.py`, 2223 lines, is 49 English phonemes each as a flat dict of Klatt
parameters. No retroflexes, no aspirates, no nasalised vowels -- so it gives
you no Urdu -- but it is the clearest existing statement of "a phoneme is a
frame", which is exactly the table openevv is missing.

Its fields map almost one for one onto our `P_` enum in `src/klatt_synth.c:21`:

    voiceAmplitude     -> P_AV          cf1..cf6   -> P_F1..P_F6
    aspirationAmplitude-> P_AH          cb1..cb6   -> P_B1..P_B6
    fricationAmplitude -> P_AF          pb1..pb6   -> P_B1F..P_B6F
    cfNP -> P_FNP   cbNP -> P_BNP       pa1..pa6   -> P_A1F..P_A6F
    cfN0 -> P_FNZ   cbN0 -> P_BNZ       parallelBypass -> P_AB
    caNP -> P_ANV

Ours is a superset: eight formants rather than six, a tracheal pole and zero
(`P_FTP` `P_BTP` `P_FTZ` `P_BTZ`), and open quotient, tilt, flutter and
diplophonia (`P_OQ` `P_TL` `P_FL` `P_DI`) that NVSpeechPlayer has no equivalent
of. Nothing it can say cannot be said in our frame.

### rsynth -- the transitions
`Elements.def`, 1950 lines. Each element carries, per parameter, a target
value and then the rank and duration of the transition into it. That second
half is the part NVSpeechPlayer's flat table does not have and the part that
decides whether a voice sounds spoken or beeped. `holmes.c` is the interpolator
that reads it. Read these two together for the frame generator.

### RHVoice -- not useful for this
Checked: 21 languages and Urdu is not among them, and `src/scripts/hts/`
confirms it is HMM-based with a vocoder, not formant. It cannot contribute a
Klatt frame or an Urdu phoneme. Kept only as a reference for how a
multi-language frontend is laid out.

---

## Second round

| Repo | Licence | Why it is here |
|---|---|---|
| BstSpeech-sapi | GPLv3 | 143 voices, 13 engines, through one SAPI5 engine. The multi-language, multi-voice, 32/64-bit problem, already solved |
| infovox230sapi5 | GPLv3 | The same again: 60 voices, 12 languages, both bitnesses |
| g2ps | **MIT** | Grapheme-to-phoneme data for 145 languages, Urdu among them. The one thing here we may actually use |
| klsyn | **non-free -- see below** | Dennis Klatt's own synthesiser. Read it, cite the papers, copy nothing |

### g2ps -- the useful one, because of its licence
Sparse checkout: Urdu, Hindi, Standard Arabic, Iranian Persian. 2.9 MB of the
294 MB. `git -C g2ps sparse-checkout add <Language>` for more.

`Urdu/Urdu_segments.csv` is 54 phonemes, each with a full articulatory feature
matrix -- forty features including `spread glottis`, which is aspiration, and
`nasal`, and `continuant`. Deriving a Klatt frame from features is a far better
starting point than guessing from an IPA symbol.

    a b b̤ cç cçʰ d̪ d̪̤ ẽ̞ f h i ĩ ĩː j k kʰ m n oː õ p pʰ q r t̪ t̪ʰ u ũː w x z
    æ̞̃ ç ŋ ɑ̃ ɑ̃ː ɔ ɖ ɖ̤ ə ə̃ ɛ ɟʝ ɟ̤ʝ ɡ ɡʰ ɣ ɪ ɽ ʈ ʈʰ ʊ ʊ̃ ʝ

Note the coronal contrast is three ways, not two: dental `t̪ d̪`, retroflex
`ʈ ɖ`, and no plain alveolar at all. English `t` is the wrong starting point
for both of them.

`Urdu/Urdu_wikipedia_symboltable.txt` is 91 grapheme-to-IPA rules over Urdu
script. Being MIT, this is a frontend we can ship, where espeak's `ur_rules`
is GPLv3 and cannot go in the tree.

### BstSpeech-sapi -- the architecture, not the voices
Thirteen engines and no Urdu among them (English classic and modern, Dutch,
French, German, Greek, Hebrew, Italian, Japanese, Polish, Portuguese, Russian,
Spanish), so it adds no language. What it has is the shape of the problem we
are about to have: a 32- and a 64-bit COM server registered into their own
registry views, and every engine DLL being 32-bit, reached from a 64-bit host
through one `b32_helper.exe` per engine -- one engine per process, audio back
over a pipe.

That last part matters beyond bitness. An engine in its own process is also
the cleanest answer to the licence question: a GPL frontend behind a pipe is
not linked into an MIT engine.

### klsyn -- careful
`README.md` carries Klatt's own restriction on `parwav.c`:

> "This software may not be resold or used in any commercial product."

That is not free, and it is a worse problem than GPL: GPL would let us ship if
we relicensed, this forbids the use outright. Read it to understand what the
parameters do. Take numbers from Klatt 1980 and the literature, never from
this file. The Python wrapper around it is BSD; the synthesiser is not.

### BstSpeech-sapi, in detail
Cloned from `joshknnd1982/BstSpeech-sapi`, which is where the work is: it and
`rommix0`'s fork are both at `20fa190c04`, while the upstream it forked from,
`gozaltech/BstSpeech-sapi`, has stood still since January at "Add ReadMe".

Four files answer questions we are about to ask. Read the design; the repo is
GPLv3, so none of it is copied.

- `src/engines.hpp` -- one `engine_info` row per language, carrying the LCID
  twice (`L"413"` and `0x0413`), the codepage, the measured sample rate, a
  loudness trim, and how many voices the frontend can actually distinguish.
  This is the table `sapi/evv_sapi.c:838` is missing: it hardcodes `L"409"`
  for all eight tokens today.
- `src/voice_attributes.hpp` -- a flat token index resolved into an (engine,
  voice) pair, with a `get_token_id()` deliberately stable across releases so
  a user's chosen voice survives an upgrade. Worth copying the discipline of.
- `src/voice_registry.hpp` -- static tokens written straight into
  `Software\Microsoft\Speech\Voices\Tokens`, not a dynamic enumerator, because
  static is what Narrator and every other client actually reads. Registration
  is factored out of `DllRegisterServer` so it can be driven against HKCU
  without elevation and therefore tested.
- `src/translit_tables.inc` and `tools/gen_translit.py` -- generated
  script-conversion tables, because the Greek and Cyrillic frontends drop
  ascii silently. Urdu is the same shape of problem seen from the other end,
  and `number_mode`, `decimal_word` and `group_sep` in `engines.hpp` are the
  per-language number reading that comes with it.

No Urdu, and no formant data anywhere in it -- the engines are closed 32-bit
dlls in `bin/`. What it is worth is the architecture, and the out-of-process
`b32_helper.exe` in particular: one engine per process, audio back over a
pipe, which is how a 64-bit host reaches a 32-bit dll and equally how an MIT
tree reaches a GPL frontend without linking it.

---

## Third round: the data the Urdu frontend actually reads

Everything above is source to read. This is data the pipeline loads at run
time, so it is listed separately and its provenance is written down word for
word -- the licence question was deferred, not answered, and answering it
later needs to be possible without working any of this out again.

| Path | Where it came from | Licence |
|---|---|---|
| `lexicons/urd_arab_broad.tsv` | WikiPron, Urdu, broad transcription | CC BY-SA 3.0 (Wiktionary) |
| `lexicons/urd_arab_narrow.tsv` | WikiPron, Urdu, narrow | CC BY-SA 3.0 |
| `lexicons/hin_deva_broad.tsv` | WikiPron, Hindi, broad | CC BY-SA 3.0 |
| `lexicons/urd_espeak.tsv` | **generated** by `tools/urdu/lexbuild.py` | derived from espeak-ng, GPLv3 |
| `wordlists/ur_full.txt` | hermitdave/FrequencyWords, `content/2018/ur` | CC BY-SA 4.0 |
| `g2ps/Urdu/` | the g2ps repo, sparse | MIT |

### The two lexicons the pipeline reads

`tools/urdu/phones.py` reads exactly two files and asks espeak only for what
is in neither. WikiPron first: 6,296 Urdu words, each a transcription a
person wrote down, and so the better answer where there is one. Then
`urd_espeak.tsv`: 10,793 words, which is espeak's answer for the same
question, written down once.

Together they cover every word of `test/cases/urdu-full.txt`, which is what
makes espeak optional at run time rather than required. That was the point of
generating it. Before, a machine without espeak dropped every unknown word
out of the sentence silently, and a machine *with* espeak lost the whole
batch whenever espeak did not answer word for word -- which it does not, for
about one word in seven.

### How `urd_espeak.tsv` was made, and how to make it again

`tools/urdu/lexbuild.py`, with `ESPEAK` naming the binary. Four minutes. The
vocabulary is the union of three things already in this directory:

- the headwords of `espeak-ng/dictsource/ur_list`, which is a hand-made Urdu
  dictionary of about three thousand entries by Him Prasad Gautam and Ejaz
  Shah -- so those entries are a person's judgement reaching us through
  espeak rather than espeak's rules;
- `wordlists/ur_full.txt`, 9,593 words by frequency off Urdu subtitles, which
  is what people say rather than what gets written down;
- every word in `test/cases/urdu-*.txt`, so that a word written to be listened
  to is never the one word that has to go to espeak at run time.

10,917 words asked, 10,793 answered. 4,517 of them had to be asked one at a
time, because the batch they were in did not come back word for word. That
number is the measurement of the fault this file exists to route around.

### Provenance, so the licence can be settled later

`urd_espeak.tsv` is machine output from a GPLv3 program run over a
GPLv3-licensed word list and a CC BY-SA one. It is a derived work of espeak
and should be treated as GPLv3 until someone decides otherwise. It is the one
file in the tree with that status, it is regenerable by one command, and
`references/g2ps/Urdu/Urdu_wikipedia_symboltable.txt` is the MIT-licensed
route to the same place if it ever has to be replaced -- 91 grapheme-to-IPA
rules over Urdu script, which is a frontend that can ship where espeak's
cannot.

The WikiPron files are Wiktionary content and so CC BY-SA: attribution and
share-alike, not a linking question, since nothing links to a TSV.

### What was consulted and not kept

`hin_deva_broad.tsv` is 33,057 Hindi words and is here because Hindi and Urdu
are one language in two scripts, so a Devanagari lexicon is in principle
33,000 more Urdu pronunciations. It is not read by anything. Converting it
needs Devanagari-to-Urdu transliteration, and that direction is the lossy
one: Urdu script does not write short vowels and spells Perso-Arabic loans
with letters Devanagari does not distinguish -- س ص ث are all one स -- so a
mechanical conversion produces a plausible spelling that is often not the
spelling anyone writes, and therefore never matches real text. Kept for the
pronunciations themselves, which are good, if a way is found to key them.
