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

The first fix was wrong, and lesson 23 is what it cost. It added a
`SynthThread.running` flag set around `stw_processRemaining` and
`addParamRun`, with `stl_stop` spinning on it and draining the app queue.
That is gone. What replaced it is in lesson 23; do not put it back.

The wrapper's part still stands: it keeps pumping after an abort so the
engine winds down before `Speak` returns, bounded, since abort makes each
callback say stop.

## 18. Write takes samples, and only samples

The engine prepended a `WAVEFORMATEX` to every `ISpTTSEngineSite::Write`, on
the theory that a buffer should say what format it is in. It should not. The
format is settled once, before any audio moves, by `GetOutputFormat` -- which
this engine already answered correctly with 11025/16/mono. A host takes the
whole buffer as PCM, so those eighteen bytes were read as nine samples of
`1, 1, 11025, 0, 22050, 0, 2, 16, 0`: an impulse every 2048 samples, which at
this rate is a click every 186 milliseconds, forever.

The rule: nothing rides along with the audio. If a site needs to know the
format, it asks.

## 19. A mock that shares the wrapper's assumption proves nothing

The reason lesson 18 survived to be shipped is that `test/sapi_smoke.c`
stripped exactly the eighteen bytes the wrapper wrote. Engine and harness
agreed with each other and with nothing else, and the harness reported a pass.

A stand-in for something must not be written from the same understanding as
the thing it is standing in for. Where the real component's behaviour is the
question, encode the *contract* -- here, "a Write is whole samples and no more
than one frame of them", which catches the header by its length -- not a
mirror of what our code happens to do.

## 20. Bytes arriving is not audio arriving

The same harness also called a run good if any bytes reached the site. Every
sample in `build/smoke.wav` was zero -- 76846 of them -- and it passed twice
and was committed as working.

Silence is the failure mode a TTS wrapper is most likely to have and the one a
byte count cannot see. Check the peak. `test/sapi_smoke.c` now fails under
`QUIET_FLOOR`, and the two checks together are what catch lessons 18 and 21:
peak alone misses the header (the real samples are still loud), and the length
check alone misses silence.

## 21. The engine's volume is not a percentage

`V_VOLUME` runs to 65535, and a voice carries about 60260 of it. SAPI's
volume, from `GetVolume` and `SPVSTATE.Volume`, is 0..100. Writing the
percentage straight through asked for a hundred parts in sixty thousand: not
quiet speech but samples that round to exact zeros, which is why the symptom
was silence rather than something faint.

`vc_getVoiceParam(h, 0, V_VOLUME)` before touching it is how to find the
voice's own level; a SAPI percentage is taken against that, the same way speed
and pitch are taken against `base_speed` and `base_pitch`. Check the scale of
every parameter that crosses between SAPI and the engine -- speed is words a
minute and pitch is hertz only because `P_REAL_WORLD_UNITS` is on, and volume
is on no such scale at all.

## 22. Latching state in a mock hides the case it was built for

`--abort-after n` decremented `writes_before_abort` to nought and left it
there, and `GetActions` aborts whenever it is nought. So the second run --
the one that asks whether an engine that was aborted still speaks, which is
the entire point of lesson 17 -- aborted on its first pump and delivered
nothing. It went unnoticed because the pass check looked at the site's
cumulative byte count, which run one had already made nonzero.

Per-run assertions need per-run state. Re-arm the site for each run, and
measure each run against where the last one ended.

## 23. The stop was not racing the worker, it was out of order

The flag from lesson 17 did not hold. NVDA cancels speech on nearly every
keystroke, and after five to ten utterances the synth host died with a read
of address zero inside the delta machine -- `ventproc`, `vinitloc_new`, a
different function each time, all of them dereferencing an arena reference
that had been zeroed under them. `--stress`, which speaks and cuts short
hundreds of times on the one engine, reproduces it in under ten runs; two
runs and a single abort never came close.

Three things were wrong with the flag:

- **It covered two functions.** Any of the `run_*` message handlers can
  reach the engine or the romanizer, not just those two.
- **It was check-then-act.** Nothing stopped the worker starting a new
  message in the moment between `stl_stop` reading zero and acting on it.
- **It gave up.** After 5000 waits it tore everything down regardless,
  which is the crash it was written to prevent.

None of that was the real problem. `stl_stop` was resetting the engine and
calling `rz_stop` *before* the `stm_qtSuspend(t)` further down -- and
`stm_qtSuspend` is the engine's own primitive for exactly this: "stop the
thread taking anything else off its queue, and wait until the one it is on
has been finished with". The teardown was simply happening in the wrong
order, and no flag could have fixed that.

The fix is two calls at the top of `stl_stop` and nothing else:

    app->vt->suspend(app);   /* the worker can no longer block posting */
    stm_qtSuspend(t);        /* and now waits out its current message */

The order matters both ways. Suspending the answer queue first is what
makes the second call finite: `q_postMessage` on a suspended queue answers
`POST_REFUSED` at once instead of queueing and waiting, so a worker part way
through handing samples over runs to the end of its message rather than
blocking forever. That is also why the `aq_poll` drain is gone -- suspending
the queue already solves what the drain was invented for.

The lesson under the lesson: when a fix needs a new flag to guard something
the codebase already has a primitive for, the fix is probably in the wrong
place. Look for the existing primitive and ask why it is not being called
early enough.

## 24. A harness that says the same thing every time cannot hear the wrong one

`--stress` spoke one sentence, hundreds of times. So when a run produced
audio, the audio was right by construction: there was nothing else it could
have been. It ran clean through the whole of the interruption work.

Then a user said it "speaks the previous utterance when prompted next
instead of the latest one". Four sentences of clearly different lengths, a
reference pass to learn what each is worth uninterrupted, and the answer was
immediate -- twelve of eighteen uninterrupted runs produced the wrong audio,
and the arithmetic says exactly what happened:

    asked for #4 (151074 bytes), got 227920   = 151074 + 76846   (#4 + #3)
    asked for #2 ( 46178 bytes), got  64724   =  46178 + 18546   (#2 + #1)

The interrupted text is never discarded. The next utterance is the leftover
with the new one appended, so a person hears what they typed before, then
what they just typed. Some runs answer with nothing at all instead.

This is not new and it is not the stop reorder of lesson 23: the engine as
of the original SAPI commit does it too, byte for byte the same
concatenation. It had simply never been looked for, because the one thing
the harness could not distinguish was one utterance from another.

Vary what the test says. A fixture that repeats itself proves that something
came out, never that the right thing did -- and "the right thing" is most of
what a speech engine is for. The same blind spot as lesson 19 and lesson 20,
in a third place: peak found silence, length found a smuggled header, and
only saying different things finds the wrong words.
