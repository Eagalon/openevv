# Urdu

`lang/urpk` is Urdu, family eighteen, and as it stands it is Italian. It was made the way `lang/plpl` was: Italian's text forms and its `rules/` copied with the tag renamed, because a language IBM never shipped has to start from one it did. It is byte for byte Italian today -- the same sentence through `lang/itit` and through `lang/urpk` is 39,875 samples and the same hash, spoken from one binary with both linked in -- and that is the statement of where it starts, not a fault. `NOTICE` says what it means for the licence, which is that all of it is IBM's Italian until something written here has replaced it.

## Why Italian

The same kind of reason Polish took it, and one more that matters more here.

Urdu's `t` and `d` are **dental**, and so are Italian's: `tools/module/phonemes.py urpk` says `ital_ph_t` and `ital_ph_d` speak at `ital_dental_Fv`, along with s, z and n. English's are alveolar, which is the wrong place for Urdu's dentals *and* leaves nothing to contrast the retroflexes against. Starting from a module whose coronals are already dental means the three-way contrast Urdu needs -- dental, retroflex, and neither -- has two of its three points already sited.

Beyond that: Urdu's `ɾ` is a tap and Italian has `ital_trill_Fv`; Urdu's vowels do not reduce and Italian's do not either; and Italian is the smallest European module, 34 phonemes and 1,749 rules against English's 3,377.

What Italian has not got that German has is `/x/` and `/ç/`, which Urdu wants for خ and for nothing else, and phonemic vowel length. Those are two known debts, written down here so the choice is not quietly forgotten.

## What is already there to build on

Six places, each a rule with formant targets in it:

    ital_labial_Fv   ital_dental_Fv   ital_velar_Fv
    ital_pal_Fv      ital_high_pal_Fv ital_trill_Fv

and 21 of the 34 phonemes have a rule of their own, `ital_ph_<name>`, which sets the source parameters and calls one of those six. That pair -- a phoneme rule and a place rule -- is the whole shape of what a new sound needs.

**The retroflex is already done once.** `lang/plpl/rules/is_val.up` has `pol_retroflex_Fv`, f2 1800 and f3 2500, arrived at by building three settings and listening to five words. It is on this same chassis, so it transfers. The note in `docs/notes/polish.md` says what it cost and what it proved: `sciarpa` through Italian and through Polish came to the same 10,197 samples with 17,448 of 20,438 bytes identical, which is one sound moved and nothing else.

## Where the Urdu comes from

`references/g2ps/Urdu/`, which is **MIT** and therefore the one source here that may be copied from rather than only read.

- `Urdu_wikipedia_symboltable.txt` -- 91 rules from Urdu script to IPA, which is the letter-to-sound half.
- `Urdu_segments.csv` -- 54 phonemes, each with a forty-column articulatory feature matrix. The columns matter: `place`, `nasal`, `continuant` and `spread glottis` are the same distinctions the phone statement's own fields carry, so a feature row can be read almost straight into what `tools/module/phonemes.py set` writes.

`references/espeak-ng` is **GPLv3** and stays a reading reference only. Its `phsource/ph_hindi_base` is the best statement anywhere of what this inventory has to cover -- aspirates as a `#` suffix, retroflexes as `.`, ten nasalised vowels -- and `dictsource/ur_rules` is a working letter-to-sound set worth understanding. Neither may be copied into this tree, and the g2ps table above is what makes that unnecessary.

## The inventory, against what the chassis has

Italian brings: b p d t k g v f z s Z S J C D T m n N G r R l L y w i e E a u o c.

What Urdu needs that is not in that list, in the order the work is worth doing:

1. **The retroflex series** -- ʈ ɖ ɽ ɳ. A place rule copied from `pol_retroflex_Fv` and a phoneme rule each. The one group with a proved precedent.
2. **The aspirates** -- pʰ tʰ ʈʰ kʰ tʃʰ. Aspiration is a source parameter rather than a place: the Klatt frame carries it as `P_AH`, and no shipped language contrasts it, so this is the first genuinely new acoustic work.
3. **The breathy series** -- b̤ d̪̤ ɖ̤ ɟ̤ ɡʰ. Breathy voice is `P_OQ` and `P_TL` in the same frame. Urdu contrasts these with both the plain and the aspirated, so a four-way stop series where Italian has two.
4. **q x ɣ** -- a uvular and two velar fricatives. German has `x` and `C` and is the place to read.
5. **The nasal vowels.** Polish declined these and speaks its ogoneks as vowel-then-n, which is what Polish itself does before a stop. Urdu's are contrastive in their own right, so the same answer would be a worse one here.
6. **Vowel length.** Urdu contrasts long and short; Italian does not.

## The first sound that is not Italian

ڑ is a retroflex flap now rather than Italian's trill.

`lang/urpk/rules/is_val.up` has `urdu_retroflex_Fv`, and the one call inside `ital_ph_R` in `is_val.dr` points at it instead of `ital_trill_Fv` -- one line changed in the lower form, which is how `lang/plpl` does it too. `R` itself is a retroflex tap in the phone records now instead of a trill, written with `phonemes.py set`.

Italian's trill was the one to take. Urdu's plain r is a tap and Italian spells that `r`; the trill `R` is what a doubled rr reaches and Urdu has no use for it, so nothing of Urdu's was spent to get it.

The numbers are 1750 and 2100, and they are deliberately not Polish's. A retroflex is a low third formant, so F2 sits a little above the dentals' 1700 and F3 comes down to meet it. `pol_retroflex_Fv` is f2 1800 and f3 2500 -- *above* Italian's palatal -- because Polish sz is a palato-alveolar its speakers hear as further back rather than a true retroflex. Urdu's are the real thing. **They have not been heard yet.** Polish's were arrived at by building three settings and listening to five words; these have had no such hearing, and when they get one the comment in the rule should say what was tried.

What it proves, measured the way Polish measured its own: `carro` through Italian and through Urdu is 9,020 samples both ways with 14,583 of 18,040 bytes the same, and `caro` through both is identical byte for byte -- 17,204 of 17,204. One sound moved and nothing else did.

## The letters, which are in

`lang/urpk/urpk.codepoints` carries 43 code points and Urdu text speaks. کتاب, پانی, اردو and سلام all come out as sound rather than as nothing.

Most characters arrive as a letter Italian already has, so its own letter-to-sound rules apply to Urdu text with nothing written. That is what makes a word audible before a single rule exists, and it is provable rather than hopeful: کتاب through Urdu and `ktab` through Italian are the same 8,899 samples and the same hash, which is what the mapping claims, since Urdu does not write its short vowels.

Urdu writes one sound several ways -- s three, z four -- and all of those arrive as the one letter. What that costs is telling them apart when spelling a word out; it costs nothing when saying one.

Five have a byte of their own, because a rule has to be able to see them and no Italian letter would say so: ٹ, ڈ and ڑ, ھ which makes the aspirates, and ں. Each says the nearest sound there is until the rule that says better is written.

Two things learned doing it, both worth not learning twice. The alphabet is keyed by the latin-1 glyph of the byte, so a byte is unusable when its glyph already names a character -- which leaves 50 of 255 free, and the eight Polish took are among them. And of those 50, 31 are below 0x20: a letter put at a control byte is dropped on the way in and the word comes back silent, with nothing said about why.
