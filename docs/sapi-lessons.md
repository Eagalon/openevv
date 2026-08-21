# Lessons: building a working SAPI 5 voice on this engine

Written while wiring `sapi/evv_sapi.c` (the ISpTTSEngine wrapper) and
`test/sapi_smoke.c`. Each entry is a thing that actually broke or nearly
broke, and the rule that came out of it. Read this before touching either
file again.

## The engine's own conventions

1. **Zero means success.** `eo_getAvailableLanguages` answers 0 when it
   worked, and -1 or nonzero when it did not -- the opposite of HRESULT.
   The CLI reads it as `if (eo_getAvailableLanguages(...) || n < 1) die();`.
   An inverted check here made every Speak fail with E_FAIL while the build
   traces all looked healthy.

2. **An empty string is a silent success.** `et_addText(h, "")` returns 1
   without queueing anything, so `et_synthesize` then completes instantly
   and `eo_speaking` answers false on its first call, with no callback ever
   fired. If synthesis finishes too fast, print the text actually handed to
   `et_addText` before suspecting the pump.

3. **Callbacks arrive inside `eo_speaking`.** Nothing drains the engine's
   queue by itself; polling is pumping. `eo_synchronizeSynth` is a no-op in
   this build (`eci_old.c`). A pump loop that never iterates means the
   engine was never given work, not that polling is broken.

4. **The instance record lives in the arena** (`eo_newEx` returned
   0x100000a0). Callback/data/buffer pointers stored through `OI_CALLBACK`,
   `OI_CBDATA`, `OI_SAMPBUF` are full-width 64-bit C fields on x86-64 --
   the offsets in `eci_old.h`'s comments describe the original 32-bit
   object, but the struct itself compiles self-consistently. Do not "fix"
   those comments into packed offsets; nothing reads them by byte.

5. **Every working caller hands the engine a process-lifetime sample
   buffer.** The CLI, `win/speak.c` and `test/dll.c` all use static arrays.
   A buffer carved out of a calloc'd COM object crashed the synth thread
   with a mangled stack (SEGV on a worker, garbage frames). Rule: the
   buffer goes in static storage, one per process, guarded by the engine
   lock -- see `frame_buf` in `sapi/evv_sapi.c`.

6. **Register the callback with real data or none.** `data=NULL` is fine
   (the CLI does it), but whatever is passed must be what the callback
   expects; during bisection a leftover NULL where `e` belonged produced a
   clean NULL deref on the first waveform report. When changing one half of
   a registration/callback pair, change both or print both.

## SAPI 5 interface facts (verified against the SDK headers)

7. **ISpTTSEngineSite layout** (from sapiddk.h): base is **ISpEventSink**
   (= IUnknown + AddEvents + GetEventInterest), then GetActions, Write,
   GetRate, GetVolume, GetSkipInfo, CompleteSkip -- in exactly that order.
   This was nearly "corrected" into the wrong order from memory; the SDK
   header is the truth, always diff against it before reordering anything.

8. **A mock site must not demand an abort by default.** The smoke test's
   `GetActions` returned SPVES_ABORT whenever `writes_before_abort == 0`,
   which is also its uninitialized value -- so the wrapper was told to
   abort on the very first pump and obeyed. Disabled now means -1, not 0.

9. **GUIDs are spelled once per translation unit.** `sapi_tts.h` lists the
   values in comments and each .c defines its own static copies; linking
   two TUs that both define the same GUID symbol fails the build.

10. **IID_ISpObjectToken is {14056589-E16C-11D2-BB90-00C04F8EE6C0}** --
    note 89, not 84; the 1405658x block is sequential across the token
    interfaces and it is easy to transcribe the wrong one.

## Building on Windows here

11. **make needs sh.** Invoke as
    `make <target> CCWIN=gcc ARWIN=ar WINDRES=windres SHELL="C:/Program Files/Git/usr/bin/sh.exe"`
    with `C:\compilers\MinGW\bin` first on PATH; otherwise recipes run under
    cmd.exe and `mkdir -p` explodes.

12. **Upstream's Makefile already knows Windows**: `CCWIN ?=
    x86_64-w64-mingw32-gcc`, objects in `build/objwin`, library
    `build/libevv-win.a`; override the tool names for the native MinGW
    install. No image-base pinning anywhere: `src/delta_low.c` translates
    language data at startup, ASLR stays on, and the old
    `--image-base/--disable-dynamicbase` plan is dead.

13. **gdb on this machine cannot enumerate modules** ("Can not parse XML
    library list"), so crash addresses stay unsymbolized and `bt` shows ??.
    Inline `-ex` commands get mangled by PowerShell quoting -- put commands
    in a file and run `gdb -batch -x file exe`. When a backtrace is useless,
    fall back to trace prints at each stage; they found every bug here.

## How the bugs were actually found

14. **Isolate by execution context.** The same sequence was run four ways:
    upstream CLI (EXE), upstream dlltest (their DLL), a raw C harness
    linked against libevv-win.a (EXE), and a raw probe exported from our
    DLL. That split "engine sequence wrong" from "DLL context wrong" from
    "COM layer wrong" in one round each. Keep `evv_sapi_raw_probe`-style
    probes in mind whenever a COM wrapper misbehaves.

15. **Diff against a working caller before theorizing.** Every divergence
    between `eng_speak`/`engine_build` and `test/dll.c` + `cli/evv.c` was
    worth one experiment: eo_new vs eo_newEx, heap vs static buffer, data
    vs NULL, param order. Two of those experiments were the whole bug.

16. **Buffered stdout lies at crash time.** printf output vanished whenever
    the process died; stderr with fflush told the truth. Trace to stderr.

## What a minimal correct Speak looks like

    engine_ensure_started();            /* port_start + static initialisers */
    eo_getAvailableLanguages(langs,&n); /* 0 == good */
    h = eo_newEx(langs[0]);
    ev_setParam(h, P_REAL_WORLD_UNITS, 1);
    vc_copyVoice(h, voice, 0);
    eo_registerCallback(h, cb, mydata); /* cb fires during eo_speaking */
    ev_setOutputBuffer(h, FRAME, static_buf);
    vc_setVoiceParam(h, 0, V_SPEED/V_PITCH/V_VOLUME, ...);
    et_addText(h, utf8);                /* "" succeeds silently! */
    et_synthesize(h);
    while (eo_speaking(h)) { check site actions; Sleep(4); }

## 17. Aborting mid-speech: the engine never learns, and eciStop races the worker

Symptoms: returning `eciDataAbort` (2) from the callback crashed the worker
thread (SEGV in delta bytecode), reproducible in a plain EXE with no COM.

Two facts explain it:

- In the buffered path the app queue's callback *is* `eo_callbackFn`, run by
  `msguser_run` on the caller's thread. Its return value becomes the queue's
  answer (`APP_ABORTED`); the ENGINE never sees `-18`. It goes on making
  samples until the text runs out.
- `eo_speaking` answers `POLL_ABORTED` by calling `eo_stop` outright, on the
  caller's thread. `stl_stop` resets the engine and stops/clears the
  romanizer while the worker may be inside `stw_processRemaining` doing
  exactly that. Nothing upstream exercises stop mid-speech, so this was
  never hit before x64 Windows testing.

Fix (three small pieces, all in our C):

1. `SynthThread.running` (new field past the original's layout): set around
   `stw_processRemaining` and `addParamRun`'s romanizer call.
2. `stl_stop` waits for `running == 0` before tearing down -- but must DRAIN
   the app queue while waiting (`aq_poll(ST_APP(t))` in the loop), or the
   undelivered samples jam the queue and hold the worker inside the engine
   forever, and the wait times out into the same crash.
3. The wrapper keeps pumping after an abort so the engine winds down before
   `Speak` returns; bounded, since abort makes each callback say stop.

After the fix: abort delivers exactly the samples written before the abort,
`Speak` returns promptly, the next utterance works, and hash.sh still says
the samples are what they have always been.
