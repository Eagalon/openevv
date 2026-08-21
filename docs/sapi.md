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

The harness drives the COM object directly, with a mock site that collects
the samples and can ask for an abort part way through. It answers zero when
every run produced audio and the abort path came back alive.

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

Aborting mid-speech is the one place the engine needed changing; see
`sapi-lessons.md` lesson seventeen for what raced and why `stl_stop` now
waits for the worker.

## Installing

`packaging/openevv-sapi.iss` builds an Inno Setup installer that registers
the right word size; `packaging/install.bat` and `uninstall.bat` do the same
job by hand from wherever the DLL sits.
