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

## Four letters that were saying the wrong thing

Routing a letter through one Italian already has is right for most of them and was plainly wrong for four, because Italian reads its own letters by their Italian rules:

- چ arrived as `c`, and Italian `c` before `a` is /k/, so چار was `car`.
- ج arrived as `g`, which before `a` is a plain /g/, so جانا was `gana`.
- ش arrived as `s`, so شام was `sam`.
- ژ arrived as `z`.

Each has a byte of its own now, saying `C`, `J`, `S` and `Z` -- the phonemes Italian already has for those exact sounds, reached directly rather than through a letter that has to be read first. Measured before and after: all three of چار, جانا and شام were byte for byte what Italian says for `car`, `gana` and `sam`, and none of them is now.

The same reading is what still spoils و and ی, which are a consonant at the start of a word and a vowel elsewhere, and both currently arrive as the vowel.

## The dictionary, and why there is not one yet

Urdu does not write its short vowels, so کتاب arrives as `ktab` and سلام as `slam`. That is the largest thing still wrong, and it is the thing the literature says is not solvable by rule: what real systems do is look the word up. openevv has the mechanism -- `lang/<tag>/<tag>.dict`, written and read back by `tools/module/dict.py` -- and `lang/urpk/urpk.dict` is dumped and reads back word for word.

It cannot yet be given Urdu words, and the reason is worth writing down rather than rediscovering:

- **A new action cannot be added.** `ital_words` and `ital_funct_words` both answer *no arm in this rule states its own record length, so a new one has nothing to copy*. Whatever the rule does with the record, it does not do it in a way the tool can copy for a word that was not there.
- **So an entry has to be retaken**, the way a phoneme is. Of `ital_funct_words`' 187 entries only **25** have an action nothing else shares; changing one of the others changes every word sharing it.
- **And a retaken action keeps its length.** It *can only be given a record of the length it already lays down*, so a five-phone Urdu word needs a five-phone Italian one. Of the 25, the lengths available are 1, 2, 6 and 7 -- there is not a single free slot of 3, 4 or 5 phones, which is most of the vocabulary worth adding.

So the dictionary is not the next move on this chassis. What is left is either a module whose dictionary rule states its lengths, or the vowels coming from somewhere other than a lexicon.

## The vowels, from espeak

The dictionary being shut on this chassis left the short vowels with nowhere to come from, and they are the largest thing wrong: کتاب read off its spelling is `ktab`.

espeak's Urdu knows them. Asked, it answers `kɪtˈaːb`, `səlˈaːm`, `mˈʊlk`, `dˈɪl`. So `tools/urdu/vowels.py` takes that answer and writes it back out as letters `lang/urpk` already says -- `kitab`, `salam`, `mulk`, `dil` -- and the engine speaks those. `tools/urdu/say.sh` is the two of them and the engine in a line.

**Nothing in the engine changed and nothing of espeak's is linked.** espeak is asked, as a separate program, and only its answer is used. The five sounds with no Latin letter in the module come back as the Urdu letter instead -- ش, چ, ج, ٹ, ڈ, ڑ, ں -- because the codepoints table already sends those to the right byte and the result stays valid UTF-8. Everything the module has not got yet falls back to the nearest it has, the same way the codepoints table does, and the diacritics for length, stress, dental, aspirated, breathy and nasal are dropped because not one of those is a distinction it can make.

A whole sentence, which is the fairest way to hear it. میرا نام احمد ہے؛ میں پاکستان سے تعلق رکھتا ہوں comes out as

    mera nam ehmad se me pakistan se taluk rakta o

which is the sentence, and says where the next work is.

**/h/ is now the loudest thing missing.** ہ and ح say nothing at all, so ہے is `e` and ہوں is `o`, and Urdu leans on that letter constantly. Italian has no /h/ to borrow and German has one; it is the first sound worth adding rather than borrowing.

After that, in what it costs a listener: the nasal vowels, which ں cannot carry on its own, so میں is `me`; and the aspirates, so رکھتا is `rakta`.

What this is not. It is a step outside the engine, so nothing reaches it through SAPI or a screen reader: a caller hands text to the engine, not to a script. Making it reach one means either the rules living in the module or the SAPI layer asking espeak the same question. And espeak's own Urdu is `status testing`: شکریہ comes back as the *names* of its letters, `ʃˈiːn kˈaːf rˈeː ...`, which is espeak reading a word it does not know letter by letter.

## The chassis, asked again after hearing it

Listening to the Italian chassis speak Urdu, the inflection is the thing that sounds wrong -- worse than US English's on the same words. That is worth settling before another sound is added, because every sound added is added to a chassis.

What is known rather than guessed:

- **Polish found the same.** `docs/notes/polish.md`: Polish stress being Italian's was *the largest single thing making the language sound Italian*, and `pol_primary_stress` in `lang/plpl/rules/it_strss.up` replaces Italian's thousand and ninety lines with sixteen. So the fix is small and there is a worked example of it.
- **But Italian's rule may be right for Urdu where it was wrong for Polish.** What Polish objected to is that Italian accents a word ending in a consonant on its last syllable. Urdu's stress is weight-sensitive and a heavy final syllable does take it -- کتاب is ki-TĀB -- so the very rule that made Polish sound Italian is close to right here. Which says the inflection that sounds wrong is more likely Italian's prosody than its stress, and prosody is a bigger thing than a rule.
- **English would bring /h/ for nothing.** English declares h and Italian has none, and /h/ is the loudest sound still missing.
- **English would cost the two things Italian was chosen for**: its coronals are alveolar where Urdu's are dental, and its vowels reduce where Urdu's do not. The second is the worse of the two -- an unstressed Urdu vowel becoming a schwa is wrong in every word.

Samples 30 to 33 are the same text through both, and the question is only answerable by ear.

A warning from the Polish note worth keeping whichever way it goes: a rule cannot answer through `get_parm_ptr`, and four separate experiments there appeared to move the stress and were all no-ops. When a change to a rule seems to work, hold it against a rule whose whole body is `match`, not only against the rule it replaced.

## The chassis question, answered: Italian stays

Heard side by side, the split is clean and it is not the one a table of features predicts.

**Italian has the better sounds.** کتاب is better on Italian than on English, and دل is wrong on English because English has not got the sounds -- its d is alveolar where Urdu's is dental, and that is audible in a word that short. Those are exactly the two things Italian was chosen for.

**English has the better inflection.** It does not have the high tone Italian speaks with. But its pronunciation is an English speaker's: the vowels are read the English way, which is the reduction Italian does not do.

So the chassis is right and the prosody is what is wrong, which is a better problem to have: the sounds are the hard part and the contour is a setting. Switching would have traded a fixable contour for unfixable vowels.

### What the high tone is, so far

Not the voice preset. `Voice1` is `0 50 65 30 0 0 50 92` in US English, in Italian and in Urdu -- the same eight numbers -- so nothing about the voice differs between the two the ear compared.

That leaves the contour itself. Italian's own intonation is six rules in `lang/urpk/rules/it_inton.dr`: `adjust_ital_inton`, `adjust_ital_accents`, `get_ital_nuclear_accent`, `adjust_ital_word_stress`, `ital_specific_word_cases` and `handle_destressed_verbs`. Two of them, `adjust_ital_accents` and `get_ital_nuclear_accent`, are called from `u_integ.dr` and are one line each to bypass, which is the same one-line change the retroflex needed and is the experiment to run next.

Two things tried first, both a setting rather than a rule: the pitch fluctuation cut from 30 to 10 in the preset, which flattens the contour without touching a rule, and the baseline pitch dropped from 65 to 50. The fluctuation change is in `urpk.settings` provisionally and comes straight back out if the ear says it is not the thing.

**Annotations are not on by default and this cost a wrong answer.** Speaking `` `vf10 `` in front of the text made the sentence two seconds longer, which is the engine reading the annotation out as words rather than applying it. Anything set that way has to be set in the preset or through the interface instead.

## The retroflex stops, 9 September 2026

ٹ and ڈ now sound, and تال/ٹال and دال/ڈال are two words apiece rather than one said twice.

They were the last structural gap. The three retroflexes of Urdu — ڑ, ٹ, ڈ — now share one locus, `urdu_retroflex_Fv`, f2 1750 and f3 2100, which is right: they differ in manner and not in place, and the third formant coming down to meet the second is the whole cue.

What took the time was where to put them. Rewriting `ital_ph_t` and `ital_ph_d` was never on: 331 and 572 lines, both calling two loci, and nearly all of it context — which shape to take before which neighbour. So the pattern that worked for ڑ and for /h/ was used again, which is to take over a sound Urdu has not got. `urdu_ph_T` and `urdu_ph_D` are written from the part of IBM's rules that makes the sound and nothing else: the two frication amplitudes that are the burst, the locus, the closing duration, and for the voiced one the store `ital_ph_z` makes and `ital_ph_s` does not, 2926.

The trade, stated plainly because it is a real loss. `N` was Italian's gn and `Z` its ʒ. Nothing in Urdu wants the first. The second is ژ, which Urdu writes in perhaps a dozen borrowed words, and it now says a ڈ; `0698` in the codepoints was moved to plain `z` so at least the letter path says something adjacent. The dentals appear in every other word and the sibilant does not, so this was not a close call.

Measured rather than assumed: `taal` against `Naal` differs over 56% of frames and `daal` against `Zaal` over 55%, against 58% for `taal` against `daal` — the new contrast is as large as the one Italian already had between t and d, which is what it should be.

## The lax vowels, and خ. 9 September 2026

Measured before changing anything. Taking the frequency list as the weight and
the lexicon as the truth, here is what share of every sound in ordinary Urdu
each engine phoneme is carrying, and what collapses onto it:

    a   22.33%   a 8.5%  ə 6.8%  ɑ 5.3%  ʌ 1.7%
    e   10.75%   e 8.6%  ɪ 2.1%
    k    9.55%   k 7.9%  x 1.0%  q 0.6%
    o    4.75%   o 3.2%  ʊ 1.4%

The second and fourth rows were a mistake of ours rather than a limit of the
chassis. ɪ went to e and ʊ to o, and that was a workaround for a problem that
had been fixed months earlier: before vowel length existed a single i was the
only i there was, Italian's is tense, and دل came out deel, so e was nearer.
Length arrived, doubling gave iː its own spelling, and the workaround stayed.

What it was costing, in the six commonest words that have one:

    دل    del      dil
    دن    den      din
    تم    tom      tum
    ملک   molk     mulk
    کتاب  ketaab   kitaab
    اردو  ordu     urduu

The name of the language, said wrong, in every sentence about it. Changed to
i and u, so ɪ and iː now differ by length alone, which is the contrast Urdu
actually makes. Measured first, because the old comment claimed a length
problem: dil is 750 ms against del's 763 and din 740 against deen's 778. A
single i is not longer than an e and never was.

خ was a /k/, which is a stop where Urdu has a fricative, and it is one per
cent of ordinary Urdu -- ten times what ڑ is, and ڑ got a locus of its own.
`urdu_ph_x' is urdu_ph_h with a place: the same two frication amplitudes and
the same voicelessness, but calling ital_velar_Fv, so the breath is forced
through the constriction /k/ is made at rather than an open tract. خدا against
کدا differs over 55% of frames.

It took v, and v is the last of them. Urdu does not contrast v with w -- و is
both -- so v measured at nought per cent before it was taken. D and T are the
only phonemes still unassigned and neither is reachable: ital_con_vals calls
them from nowhere, so a code put there makes no sound at all. Adding a branch
to that dispatch is what a further sound would cost, and it is the reason غ is
still a /g/.

ق stays a /k/ on purpose. Most of Pakistan says it that way.

## The lexicon reaches the voice. 9 September 2026

Everything the frontend knew lived in tools/urdu/phones.py, which is Python
and runs offline. The voice a person installs never saw any of it. SAPI hands
the engine letters, Urdu does not write its short vowels, and so the voice
guessed at the vowel in every word while a lexicon that knew it sat unused on
disk.

The lookup is the whole of what had to cross, and it crosses as data rather
than as code. `tools/urdu/gen_sapi_lex.py` writes, for every word, what
phones.py finally decides -- after the IPA is mapped, after length is doubled,
after the stress rule has run -- as the phoneme string it ends up being. So
`sapi/urdu_text.c` does no phonetics at all. It splits words, normalises the
spelling, reads numbers, and wraps the answer in the annotation the engine
already reads. 102,453 words, 2.3 MB of blob and 0.4 MB of index; a strcmp
binary search finds what Python would have found, because Python sorts by code
point and UTF-8 compares byte for byte in the same order.

Two implementations of one thing is how a thing comes to disagree with itself,
so `test/urdu_agree.sh` runs the same text through both and diffs. It says
nothing about whether the pronunciation is good -- that is what the ear is for
-- only whether the voice says what the samples say, which is a question a
diff can answer and a listener cannot. Both cases agree.

Better than agreeing: the audio is identical. The whole test file spoken
through the new path and through the WAV pipeline is the same file, byte for
byte.

What it fixed beyond the vowels. ہ was the last silent letter that mattered --
it arrives as Italian's h, which Italian does not pronounce, and it is
everywhere in Urdu. It needed a letter rule, and now it does not: ہے، ہوں،
ہم، یہ، وہ، کہا all come out of the lexicon with the /h/ phoneme in them. The
letter rule was the wrong fix for a problem the lookup does not have.

The text path was fuzzed before it was believed: 300 random byte strings and
every truncation of a valid Urdu sentence, no failures. Empty input,
punctuation alone, English, a six-hundred-character word and a word of nothing
but ژ all come back without a crash. A word the lexicon has not got is dropped
rather than guessed at, which is what phones.py does and for the same reason:
an annotation naming a phoneme the module has not got is spoken aloud,
backticks and all.

## Four and a half per cent of Urdu was being deleted. 9 September 2026

The shortcomings were listed and then measured, which changed which of them
mattered. Weighted by how often the words carrying them are said, here is
every IPA symbol that fell through PAIRS and was dropped in silence:

    ɟ   U+025F   2.391%   ج
    ʱ   U+02B1   2.214%   the breathy voice of بھ دھ گھ
    .   U+002E   0.696%   espeak's retroflex marker, which is ڑ
    ʂ   U+0282   0.181%
    ᵊ   U+1D4A   0.117%
    ʐ   U+0290   0.114%

The first is the worst thing found in this branch. espeak writes ج as a bare
`ɟ` and the table had only `ɟʝ`, so it matched nothing and the consonant was
deleted: جلدی came out `aldii`, جان came out `aan`. Two and a half per cent of
all Urdu speech with a consonant simply missing from it, and no error
anywhere, because a symbol that matches no rule is skipped by design.

The second is Urdu's breathy voiced stops. `ʰ` and `̤` were in ASPIRATE and
`ʱ` was not, so آدھا was `aadaa`, بھائی was `baaii`, دودھ was `duud`.

The third is the one that stings. espeak writes ڑ as an r with a full stop
after it -- its own notation leaking into what claims to be IPA -- and 4,836
words of the lexicon carry one. The r matched, the stop fell through, and
every ڑ in an espeak-derived word came out a plain tap. The retroflex locus
written for ڑ, and proved against carro and caro, was only ever reached by
WikiPron's tenth of the lexicon.

All of them are mapped now and the drop table is empty but for one word in a
hundred thousand. This is what auditing costs against what guessing costs: an
hour of measurement found more than a day of listening had.

## A word not known is now said badly rather than not said

An unknown word used to be dropped. The defence was that an annotation naming
a phoneme the module has not got is spoken aloud, backticks and all -- which
is an argument against guessing at phonemes, not an argument for silence. The
lexicon covers 99.3% of ordinary Urdu by token and the other 0.7% is where the
names are. A person's own name coming back as nothing at all is the worst
thing this can do.

The engine reads plain text in annotation mode -- measured: a raw word between
two annotations adds its own length to the utterance -- so an unknown word now
goes through as its own letters and gets the letter-by-letter reading the
whole voice used to give. It has no short vowels in it and it is not right.
زولفقار and ٹیلیوژن in a test sentence were 0.92 seconds of silence and are
now 0.92 seconds of speech.

## Stress was checked and left alone

Twelve words whose stress is not in doubt, against what the rule gives:
کتاب، پاکستان، معلوم، دروازہ، مسلمان، استعمال، تعلیم، کھانا، لکھنا، اردو and
پانی all come out where a speaker puts them. Only لڑکی differs, and which of
LAR-ki and lar-KI is right is genuinely disputed. Eleven of twelve is not a
rule to churn for the twelfth.

## fikar, and espeak's Hindi showing through. 9 September 2026

Reported by ear: فکر was being said fikr and an Urdu speaker says fikar. That
is not one word, it is a rule, and it is one of the things that most marks a
speaker as native.

Urdu breaks up a word-final consonant cluster when the second consonant is
more open than the first: فکر fikar, صبر sabar, عقل aqal, شکل shakal, حکم
hukam, ختم khatam, اصل asal, نظم nazam. It leaves one alone when the sonority
does not rise: دوست dost, وقت waqt, بند band, پسند pasand are all said as
written. `epenthesis()` in phones.py reads sonority rather than letters, which
is what makes both halves of that come out right.

Neither source does it reliably. WikiPron writes the vowel in عمر and صبر and
اسم and not in فکر or حکم or عقل; espeak disagrees with it about which. So the
rule is applied here, uniformly, rather than hoped for from the data. The
vowel is a schwa, which is what WikiPron writes wherever it writes one:
عمر is ʊməɾ and صبر is səbəɾ.

An aspirate is not a cluster and the rule has to know it. کچھ is written kuCL
because the module has no aspirated affricate and says one as a stop and a
breath, so a rule reading sonority would happily put a vowel in the middle of
one sound. Sixteen words tested, nine that should take a vowel and seven that
should not; all sixteen right.

The dialect. espeak's Urdu is built on its Hindi and one place that shows is
the final ی. Urdu says it long -- آزادی aazaadii, زندگی zindagii, لڑکی larkii
-- and espeak writes a short i. Counted against WikiPron over every word both
know: 698 words end in ی, WikiPron makes 683 long, and espeak makes 494 of
those short. Seventy-two per cent wrong, on the ending of every feminine and
every abstract noun in the language, across the nine tenths of the lexicon
that comes from espeak. Words ending in ی are nine per cent of everything
said.

Only ی. The same count over ا, ے and و finds espeak short in none of them, so
`mend()` corrects the one thing it is reliably wrong about and does not
second-guess it generally. The fifteen words where WikiPron says a final ی is
not a long i are all Arabic, where it is an alif maqsura and says aa -- اعلی،
دعوی، موسی، یحیی -- and every one of them is in WikiPron, which is read first
and wins.

And a fault of mine from an hour earlier, found by looking rather than
reasoning. ʂ was sent to S on the guess that a retroflex sibilant is near ش.
It is not: ʂ is how espeak writes ص, which in Urdu is a plain s, so صبر said
shabar and صاف said shaaf. Which letters give rise to a symbol is a thing to
look up in the lexicon, not to reason about from the symbol's name.

## The duration model is Italian's, and where it lives. 9 September 2026

Reported by ear, and correctly: the dialect problem is not the phonemes, it is
that some sounds are stretched and others cut in the way Italian and Spanish
do it. Italian lengthens a stressed vowel in an open syllable and cuts an
unstressed one hard. Urdu does neither -- its vowel length is in the word, not
in the position -- so an Urdu word built out of Italian durations sounds like
an Italian reading Urdu however right the phonemes are.

Measured: four identical `a' phonemes in `takatakata' come back 110, 45, 45
and 65 milliseconds. Same phoneme, same neighbours, a two-and-a-half-fold
spread, and moving the stress mark makes it 3.7-fold. Nothing in the
annotation asked for any of that.

Where it lives. `assign_ital_nuc_durs' in is_sidur.dr runs a pipeline: a
starting duration from `assign_stanital_start_dur', then six adjustments --
`ital_context_adjust', `syllable_sequence_adjust', `syll_phone_adjust',
`word_syll_adjust', `phrase_final_adjust' -- and `distribute_nucdur' to spread
the total over the phones. Every one is a single call at a known line, so each
can be replaced with a rule that does nothing, which is what is_sidur.up now
holds: three no-ops, one per arity the pipeline uses.

Tried one at a time. Three of the six -- `ital_context_adjust',
`syll_phone_adjust' and `word_syll_adjust' -- change nothing whatever on this
input, to the sample: they are guarded by tests our annotations never reach.
The two that fire are `syllable_sequence_adjust' and `phrase_final_adjust'.

Which of them should go is not a question this can answer by measuring. The
spread metric above is built on voiced runs in the waveform and it does not
segment vowels reliably -- the run count changes when a rule is disabled,
which means it is tracking something other than the vowels. So the four
combinations are rendered in dist/urdu-samples as R1 to R4 and the ear
decides. They differ from each other over 92% of frames, so there is a real
choice there and not four versions of one thing.

Nothing is changed in the tree by this. is_sidur.dr is at baseline and the
installer speaks R1. The no-ops are committed because building them was most
of the work and the next round should not have to do it again.
