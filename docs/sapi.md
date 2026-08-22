# The SAPI5 engine

`sapi/evv_sapi.c` is a Windows SAPI5 text-to-speech engine: a COM object
implementing `ISpTTSEngine`, with eight voices registered under
`HKLM\SOFTWARE\Microsoft\Speech\Voices\Tokens\OpenEloquence.N`. One DLL,
self-contained, no data files. `sapi/sapi_tts.h` is a self-contained SAPI5
header so the build needs no Windows SDK.

## Building

    make sapi          # build/OpenEloquence.dll, sixty-four bit
    make sapi-test     # build/sapi_smoke.exe, the test harness
    make sapi32        # build/OpenEloquence32.dll, wants a 32-bit compiler

The DLL links the same objects as `eci.dll` plus the wrapper itself.

## Testing

    ./build/sapi_smoke.exe                 # two utterances to build/smoke.wav
    ./build/sapi_smoke.exe -v 3            # voice three
    ./build/sapi_smoke.exe -r 6            # rate six
    ./build/sapi_smoke.exe --abort-after 2 # site asks for abort after two writes
    ./build/sapi_smoke.exe --stress 200    # speak and cut short, two hundred times

The harness drives the COM object directly, with a mock site that collects
the samples and can ask for an abort part way through. It answers zero when
all of these hold, and names the one that did not otherwise:

  * every run delivered audio, and that audio has a peak amplitude in the
    thousands -- speech off this engine reaches about twenty thousand, and a
    run of bytes that are all zero is the failure this check exists for;
  * every `Write` was whole samples and no more than one frame of them, which
    is what catches anything prepended to the audio;
  * a stream started and ended for each run.

With `--abort-after n` the first run is cut short and the second is left
alone, so the pass says both that the abort was obeyed and that the engine
still speaks afterwards.

`--stress N` is the one that found the crash a screen reader hits: N runs on
the one engine, most of them cut short at a different point, which is what
NVDA does on every keystroke. Audio is not checked there -- most runs are
meant to be truncated -- only that the engine is still alive and answering.
Two runs and one abort never reached it; ten stress runs did.

It says four sentences of very different lengths rather than one, and
measures each uninterrupted first, so it can tell which of them actually
came out. That is how it catches an utterance the engine kept from before
the last interruption -- a thing a fixture that repeats itself can never
see. **It fails today**: the interrupted text is not discarded, and the next
utterance is that leftover with the new one appended. See
`sapi-lessons.md` lesson twenty-four; the failure predates the stop work and
is not fixed.

It also times what a person actually feels, which is not how long an
utterance takes but how long the engine takes to react:

  * how long from the site demanding an abort to `Speak` coming back;
  * how long from `Speak` being called to the first sample reaching the site.

Both sit near twenty milliseconds and neither drifts over hundreds of runs.
`--sink-delay N` makes the site block N milliseconds a buffer, which is what
a real one does and the mock otherwise never does; the abort figure does not
move under it, because an aborted callback stops writing rather than waiting
its turn. If either number grows, something has been made slower, and these
are the runs that say so.

An access violation anywhere in the harness now names the module it happened
in and how far into it, because gdb here cannot enumerate modules and a bare
"Segmentation fault" says nothing. `nm` on the DLL turns that offset into a
function name.

Neither check is redundant: a peak alone would not notice a header riding in
front of the samples, since the samples themselves are still loud, and a
length check alone would not notice silence.

For the whole stack, register and speak through Windows itself:

    regsvr32 build\OpenEloquence.dll

then any SAPI5 client sees eight "Open Eloquence" voices. Unregister with
`regsvr32 /u`.

## How it fits together

The wrapper keeps one engine instance per COM object, built lazily on the
first `Speak`: language list, `eo_newEx`, voice copied into slot nought,
callback registered, static sample buffer handed over -- in that order, since
the engine refuses a buffer until it has somewhere to report samples.

`Speak` walks the fragment list, converts each piece to UTF-8, sets speed in
words a minute and pitch in hertz (SAPI's rate and pitch adjustments are
semitone offsets around the voice's own base), queues it, then pumps
`eo_speaking` until quiet. Between pumps it reads the site's actions: abort
and skip both stop the run, skip answering `CompleteSkip` with the items
asked for. Samples arrive in the callback, which writes them to the site as
`SPDF_Speech` data at eleven kilohertz, mono, sixteen bits.

Aborting mid-speech is the one place the engine needed changing: `stl_stop`
was resetting the engine and stopping the romanizer before it suspended the
worker's queue, so a cancel landing mid-utterance tore both out from under
the thread using them. It suspends first now. A screen reader cancels on
nearly every keystroke, which is what found it; `sapi_smoke.exe --stress N`
is that shape, and it reproduces the old crash in under ten runs. See
`sapi-lessons.md` lessons seventeen and twenty-three.

## Installing

    make installer     # dist/OpenEloquence-SAPI5-setup.exe

`packaging/openevv-sapi.iss` takes whichever engines are in `build/`: the
sixty-four bit one, the thirty-two bit one, or both. Nothing has to be edited
when only one was built, so `make sapi installer` is enough on a machine with
no thirty-two bit compiler. It wants Inno Setup 6 -- `winget install
JRSoftware.InnoSetup` -- and `ISCC=...` says where, if it is not where winget
leaves it. `ISCC /DAppVersion=1.2` sets the version.

`packaging/install.bat` and `uninstall.bat` do the same job by hand from
wherever the DLLs sit, for a copy that is not being installed. Both need to
be run as administrator and say so plainly rather than failing quietly, and
both report which word size was registered and whether the voices actually
appeared afterwards.

On a sixty-four bit Windows both engines belong on the machine, not one or
the other: a thirty-two bit program asking Windows for voices sees only
thirty-two bit engines. Each is registered by the `regsvr32` of its own word
size, which is also what puts the thirty-two bit entries under `Wow6432Node`
where a thirty-two bit host looks for them.
