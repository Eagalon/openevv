/* Interrupt the engine over and over, and say what came out.
 *
 * The SAPI wrapper is where stopping mid-speech goes wrong, and the SAPI
 * wrapper only exists on Windows -- where gdb on the machine this was
 * written on cannot enumerate modules, so a crash is question marks. This
 * does the same thing to the engine through the plain library, on whatever
 * this is built for, so the fault can be looked at with a debugger that
 * works and run under a sanitiser.
 *
 * What it does is what a screen reader does: say something, cut it off part
 * way through, say the next thing. The engine learns of the cut through the
 * answer its callback gives, which is how eciStop comes to be called from
 * inside eciSpeaking.
 *
 * Four sentences of very different lengths go round in turn, and each is
 * measured once with nothing interrupting it, so a run that comes back the
 * size of a different one is an utterance the engine kept from before the
 * last interruption -- which cannot be seen at all if every run says the
 * same words.
 *
 *   stopstress [runs]        default 200
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evv_abi.h"

typedef struct OldInst OldInst;

enum ECIMessage { eciWaveformBuffer, eciPhonemeBuffer, eciIndexReply };
enum ECICallbackReturn { eciDataNotProcessed, eciDataProcessed, eciDataAbort };

enum { P_REAL_WORLD_UNITS = 8 };
enum { V_GENDER, V_HEAD_SIZE, V_PITCH, V_FLUCTUATION, V_ROUGHNESS,
       V_BREATHINESS, V_SPEED, V_VOLUME, V_COUNT };

OldInst *STDCALL eo_newEx(int32_t lang);
int      STDCALL eo_getAvailableLanguages(uint32_t *langs, int *n);
void     STDCALL eo_registerCallback(OldInst *h, void *cb, void *data);
int      STDCALL ev_setOutputBuffer(OldInst *h, int32_t n, void *buf);
int      STDCALL ev_setParam(OldInst *h, int32_t which, int32_t value);
int      STDCALL et_addText(OldInst *h, const char *text);
int      STDCALL et_synthesize(OldInst *h);
int      STDCALL eo_speaking(OldInst *h);
OldInst *STDCALL es_delete(OldInst *h);

void evvRunStaticInitialisers(void);
void evv_port_start(void);

#define FRAME 2048

/* Process-lifetime, as every caller of this engine that works uses. */
static short frame_buf[FRAME];

static int    trace;
static size_t got;              /* samples this utterance */
static int    cut_after;        /* stop after this many buffers, -1 never */
static int    buffers;

static enum ECICallbackReturn STDCALL on_message(OldInst *h,
                                                 enum ECIMessage msg,
                                                 long param, void *data)
{
    (void)h;
    (void)data;
    if (msg != eciWaveformBuffer)
        return eciDataProcessed;
    buffers++;
    if (trace)
        fprintf(stderr, "    buffer %d: %ld samples\n", buffers, param);
    if (cut_after >= 0 && buffers > cut_after)
        return eciDataAbort;
    got += (size_t)param;
    return eciDataProcessed;
}

/* One utterance. Answers the samples it delivered. */
static size_t say(OldInst *h, const char *text, int cut)
{
    got = 0;
    buffers = 0;
    cut_after = cut;
    if (trace) {
        fprintf(stderr, "== say \"%.20s\" cut=%d\n", text, cut);
        fflush(stderr);
    }

    if (!et_addText(h, text) || !et_synthesize(h)) {
        fprintf(stderr, "stopstress: it refused the text\n");
        exit(2);
    }
    while (eo_speaking(h))
        usleep(2000);
    return got;
}

int main(int argc, char **argv)
{
    static const char *const lines[] = {
        "One.",
        "One two three four five.",
        "The quick brown fox jumps over the lazy dog.",
        "Pack my box with five dozen liquor jugs, and then pack another box "
        "with five dozen more, quickly."
    };
    const int n_lines = (int)(sizeof lines / sizeof lines[0]);
    size_t expect[4];
    uint32_t langs[8];
    int n = 8, i, runs, wrong = 0, silent = 0, checked = 0;
    OldInst *h;

    runs = argc > 1 ? atoi(argv[1]) : 200;
    trace = getenv("EVV_TRACE") != 0;

    evv_port_start();
    evvRunStaticInitialisers();

    if (eo_getAvailableLanguages(langs, &n) != 0 || n < 1) {
        fprintf(stderr, "stopstress: it has no language in it\n");
        return 2;
    }
    h = eo_newEx((int32_t)langs[0]);
    if (h == 0) {
        fprintf(stderr, "stopstress: it would not make an instance\n");
        return 2;
    }
    ev_setParam(h, P_REAL_WORLD_UNITS, 1);
    eo_registerCallback(h, (void *)on_message, 0);
    if (!ev_setOutputBuffer(h, FRAME, frame_buf)) {
        fprintf(stderr, "stopstress: it refused a sample buffer\n");
        return 2;
    }

    for (i = 0; i < n_lines; i++) {
        expect[i] = say(h, lines[i], -1);
        printf("reference %d: %lu samples\n", i + 1,
               (unsigned long)expect[i]);
    }

    for (i = 0; i < runs; i++) {
        int which = i % n_lines;
        int cut = (i % 7 != 0) ? (i % 13) + 1 : -1;
        size_t n_got = say(h, lines[which], cut);

        if (cut >= 0)
            continue;               /* meant to be cut short */
        checked++;
        if (n_got == expect[which])
            continue;
        wrong++;
        if (n_got == 0) {
            silent++;
            printf("  run %d: asked for #%d, got nothing\n", i + 1,
                   which + 1);
        } else {
            int j;

            for (j = 0; j < n_lines; j++)
                if (j != which && n_got == expect[j])
                    break;
            if (j < n_lines)
                printf("  run %d: asked for #%d, got #%d\n", i + 1,
                       which + 1, j + 1);
            else
                printf("  run %d: asked for #%d (%lu), got %lu\n", i + 1,
                       which + 1, (unsigned long)expect[which],
                       (unsigned long)n_got);
        }
        fflush(stdout);
    }

    es_delete(h);
    printf("uninterrupted runs checked: %d, wrong: %d (silent: %d)\n",
           checked, wrong, silent);
    return wrong ? 1 : 0;
}
