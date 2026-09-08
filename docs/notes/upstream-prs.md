# Two things this branch fixed that belong upstream

Both are defects in the tree as it stands on `main`, and both are invisible from Linux, which is why they are still there. Neither has anything to do with SAPI: they are in the Makefile and in the tools, and a Windows build of `main` alone runs into both. They are written down here so a pull request can be made from this without working any of it out again.

Neither is urgent for anyone building on Linux. Both stop a Windows build dead.

## 1. Every archive rule spells its object list onto one command line

**Where.** The five archive rules in `Makefile`: `$(BUILD)/$(SONAME)`, `libevv$(SUF).a`, `libevv32$(SUF).a`, `libevv-win$(SUF).a` and `libevv-win32$(SUF).a`.

Each of them names every object twice, once in the sweep that removes stale objects and once for `ar`:

```make
	@for o in $(OBJDIR)/*.o; do \
	   case " $(OBJECTS) " in *" $$o "*) ;; *) rm -f "$$o" ;; esac; \
	 done
	@rm -f $@
	@ar rcs $@ $(OBJECTS)
```

**What goes wrong.** One language is 141 objects and ten are 267, and each is a path of some length. On Windows that is past what a command line carries. What it does there is the part worth knowing: it is not a length error and nothing says the word "long". The shell is handed a string cut off in the middle of a path and stops on the quote that string now ends with, so the build dies with

```
/usr/bin/sh: -c: line 2: unexpected EOF while looking for matching `"'
make: *** [Makefile:694: build/libevv-win-...-plpl.a] Error 2
```

which reads like a quoting mistake in the Makefile and is not one. It appears only above some number of languages, so a one-language build on the same machine is fine and the fault looks like something about the language that was added.

**The fix, which is on this branch.** A `write_objs` helper built on `$(file ...)`, which writes without a shell and so has no length limit at all:

```make
define write_objs
$(file >$1,)$(foreach o,$2,$(file >>$1,$o))
endef
```

Each rule then writes its list once and both halves read that file -- the sweep with `grep -qxF`, and `ar` with `@` in front of the name, which GNU `ar` takes as a response file. `$(file ...)` wants GNU make 4.0 or newer, which the flake has.

**Worth saying in the PR** that this is not only a Windows fix. Linux has a much larger limit rather than no limit, and the list grows with both the language count and the length of the build path.

## 2. The Python tools read files in whatever encoding the platform defaults to

**Where.** `tools/rules/notation.py` is the one that stops a build, because every build runs it: `make` writes each module's rules out of `lang/<tag>/rules` before compiling anything. It calls `open()` about a dozen times and names an encoding in none of them. It is not alone -- 29 of the 39 Python files under `tools/` never name one -- but the others are lifters and analysers that a build does not run.

**What goes wrong.** Python uses the platform default, which is UTF-8 on Linux and cp1252 on a Western Windows. The rules as text are UTF-8 and Polish's are not ASCII, so:

```
UnicodeDecodeError: 'charmap' codec can't decode byte 0x81 in position 7841:
character maps to <undefined>
make: *** [Makefile:811: lang/plpl/delta_rules_plpl.h] Error 1
```

An English-only build gets past it, so this too looks like something wrong with the language rather than with the tool.

**Two ways to fix it.** The narrow one is `encoding="utf-8"` on every `open()` in `notation.py` and in whatever else a build runs, which is explicit and cannot be got round by the environment. The broad one is to say the tree is UTF-8 once, in `tools/evv.py`, which everything imports. The narrow one is the one to offer: it changes only the files a build touches and says the encoding where the file is opened.

**What this branch does instead.** Nothing to the tools. It puts a `python3` on the path that sets `PYTHONUTF8=1`, which is a local workaround and deliberately not a change to upstream's files. It is worth saying in the PR that the workaround exists, because it explains how the tree was built on Windows at all.

## Not for upstream

The SAPI5 engine in `sapi/`, its installer in `packaging/`, and the language work in `test/sapi_smoke.c` are this branch's own and upstream has no SAPI layer to put them in. The `delta_lang_bind_all` bug they turned up is ours as well: nothing in upstream reads `delta_languages[]` from outside the engine, so nothing there can hit it.
