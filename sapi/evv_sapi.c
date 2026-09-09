/* SAPI 5 front end for the engine.
 *
 * One COM in-process server, one engine class, eight voice tokens. SAPI
 * parses whatever XML the text carried before we ever see it and hands us a
 * list of fragments, so what is left to do here is: turn each fragment into
 * text and voice settings, queue it, pump the engine while it speaks, and
 * hand every buffer of samples it makes to the site.
 *
 * The engine's audio is 11025 Hz mono sixteen-bit and nothing here changes
 * that; GetOutputFormat answers with exactly that so SAPI's wave sink is set
 * up for it before the first Write.
 */

#include <windows.h>
#include <mmreg.h>
#include <objbase.h>
#include <olectl.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sapi_tts.h"
/* Bare names, as everything in src is included: the Makefile works the
   include path out from the directories on disk, so a header that moves
   between groups -- both of these did -- costs no line here. */
#include "evv_abi.h"
#include "delta_lang.h"

typedef struct OldInst OldInst;

enum ECIMessage {
    eciWaveformBuffer,
    eciPhonemeBuffer,
    eciIndexReply
};

enum ECICallbackReturn {
    eciDataNotProcessed,
    eciDataProcessed,
    eciDataAbort
};

/* The engine's own parameters, and a voice's. Only the few this needs. */
enum { P_REAL_WORLD_UNITS = 8 };
enum { V_GENDER, V_HEAD_SIZE, V_PITCH, V_FLUCTUATION, V_ROUGHNESS,
       V_BREATHINESS, V_SPEED, V_VOLUME, V_COUNT };

OldInst *STDCALL eo_new(void);
OldInst *STDCALL eo_newEx(int32_t language);
int      STDCALL es_delete(OldInst *h);
int      STDCALL et_addText(OldInst *h, const char *text);
int      STDCALL et_synthesize(OldInst *h);
int      STDCALL ev_setOutputBuffer(OldInst *h, int32_t n, void *buf);
int32_t  STDCALL ev_setParam(OldInst *h, int32_t which, int32_t value);
int32_t  STDCALL vc_getVoiceParam(OldInst *h, int32_t voice, int32_t which);
int      STDCALL vc_setVoiceParam(OldInst *h, int32_t voice, int32_t which,
                                   int32_t value);
int      STDCALL vc_copyVoice(OldInst *h, int32_t from, int32_t to);
void     STDCALL eo_registerCallback(OldInst *h, void *cb, void *data);
void     STDCALL eo_synchronizeSynth(OldInst *h);
int      STDCALL eo_speaking(OldInst *h);
int      STDCALL eo_getAvailableLanguages(uint32_t *out, int *count);

void evvRunStaticInitialisers(void);
void evv_port_start(void);

/* The formant voice runs at eleven thousand and twenty-five samples a second
   and nothing here changes that. */
#define RATE  11025
#define FRAME 2048

/* {370245F1-D510-4895-A2C4-4C296A2A8FD5} */
static const GUID CLSID_OpenEloquence = {
    0x370245f1, 0xd510, 0x4895,
    {0xa2, 0xc4, 0x4c, 0x29, 0x6a, 0x2a, 0x8f, 0xd5}
};

#define EVV_GUID(l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

static const GUID IID_ISpTTSEngine_ = EVV_GUID(0xa74d7c8e, 0x4cc5, 0x4f2f,
    0xa6, 0xeb, 0x80, 0x4d, 0xee, 0x18, 0x50, 0x0e);
static const GUID IID_ISpTTSEngineSite_ = EVV_GUID(0x9880499b, 0xcce9, 0x11d2,
    0xb5, 0x03, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96);
static const GUID IID_ISpObjectWithToken_ = EVV_GUID(0x5b559f40, 0xe952,
    0x11d2, 0xbb, 0x91, 0x00, 0xc0, 0x4f, 0x8e, 0xe6, 0xc0);
static const GUID SPDFID_WaveFormatEx_ = EVV_GUID(0xc31adbae, 0x527f, 0x4ff5,
    0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);

#undef EVV_GUID

static const wchar_t CLSID_STRING[] =
    L"{370245F1-D510-4895-A2C4-4C296A2A8FD5}";
static const wchar_t TOKENS_KEY[] =
    L"SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens";
static const wchar_t VOICE_PREFIX[] = L"OpenEloquence.";

/* The eight voices are the eight presets the language data ships with. The
   genders follow the ini; if that ever changes, this table is the only thing
   here that has to hear about it. */
static const struct {
    const wchar_t *name;
    const wchar_t *gender;
} evv_voices[8] = {
    { L"Open Eloquence 1", L"Male"   },
    { L"Open Eloquence 2", L"Female" },
    { L"Open Eloquence 3", L"Female" },
    { L"Open Eloquence 4", L"Male"   },
    { L"Open Eloquence 5", L"Male"   },
    { L"Open Eloquence 6", L"Female" },
    { L"Open Eloquence 7", L"Female" },
    { L"Open Eloquence 8", L"Male"   }
};

/* The engine numbers a language as a family and a dialect packed into a
   word; Windows numbers it as an LCID. Nothing in the engine knows the
   second and nothing in it should, so the two are held together here and
   nowhere else.
 *
 * Latin American Spanish and Canadian French are the two that have to be
 * chosen rather than read off: the engine says the region and Windows wants
 * a country, so es-MX and fr-CA stand for them. A language missing from
 * this table is still published, under the neutral LCID for "the user's
 * own", rather than being left out of the voice list. */
static const struct {
    int32_t        id;
    const wchar_t *lcid;
} evv_lcids[] = {
    { 0x010000, L"409"  },   /* US English             en-US */
    { 0x010001, L"809"  },   /* British English        en-GB */
    { 0x020000, L"c0a"  },   /* Castilian Spanish      es-ES */
    { 0x020001, L"80a"  },   /* Latin American Spanish es-MX */
    { 0x030000, L"40c"  },   /* French                 fr-FR */
    { 0x030001, L"c0c"  },   /* Canadian French        fr-CA */
    { 0x040000, L"407"  },   /* German                 de-DE */
    { 0x050000, L"410"  },   /* Italian                it-IT */
    { 0x080000, L"411"  },   /* Japanese               ja-JP */
    { 0x110000, L"415"  },   /* Polish                 pl-PL */
    { 0x120000, L"420"  }    /* Urdu                   ur-PK */
};

static const wchar_t *lcid_of(int32_t id)
{
    size_t i;

    for (i = 0; i < sizeof evv_lcids / sizeof evv_lcids[0]; i++)
        if (evv_lcids[i].id == id)
            return evv_lcids[i].lcid;
    return L"400";
}

/* The module's own name is eight bit and the registry wants sixteen. The
   names are ASCII, so this is a widening and not a conversion. */
static void widen(const char *s, wchar_t *out, size_t room)
{
    size_t i = 0;

    while (s[i] != 0 && i + 1 < room) {
        out[i] = (wchar_t)(unsigned char)s[i];
        i++;
    }
    out[i] = 0;
}

typedef struct EvvEngine EvvEngine;

/* The sample hand-off. One per process, guarded by the engine lock: the
   engine wants the address of a place to put samples, and every caller of
   this engine that works gives it one that lives for the process, not one
   that moves. */
static short frame_buf[FRAME];

struct EvvEngine {
    ISpTTSEngine       tts;
    ISpObjectWithToken with_token;
    LONG               refs;
    CRITICAL_SECTION   lock;
    ISpObjectToken    *token;
    int                voice;
    /* Which language this token names, packed as the engine has it. Nought
       until a token says, and then the first the build carries. */
    int32_t            lang;
    OldInst           *h;
    ISpTTSEngineSite  *site;
    ULONG              stream_num;
    volatile LONG      aborting;
    volatile LONG      speaking;
    int                base_speed;
    int                base_pitch;
    int                base_volume;
    ULONGLONG          audio_offset;
};

static HRESULT STDMETHODCALLTYPE eng_query_interface(ISpTTSEngine *self_,
                                                     REFIID riid, void **out);
static ULONG STDMETHODCALLTYPE eng_addref(ISpTTSEngine *self_);
static ULONG STDMETHODCALLTYPE eng_release(ISpTTSEngine *self_);
static HRESULT STDMETHODCALLTYPE eng_speak(ISpTTSEngine *self_, DWORD flags,
                                           REFGUID fmtid,
                                           const WAVEFORMATEX *wfx,
                                           const SPVTEXTFRAG *frags,
                                           ISpTTSEngineSite *site);
static HRESULT STDMETHODCALLTYPE eng_get_output_format(ISpTTSEngine *self_,
                                                       const GUID *tfid,
                                                       const WAVEFORMATEX *twfx,
                                                       GUID *ofid,
                                                       WAVEFORMATEX **owfx);
static HRESULT STDMETHODCALLTYPE eng_get_status(ISpTTSEngine *self_,
                                                SPVOICESTATUS *status);

static const ISpTTSEngineVtbl eng_tts_vtbl = {
    eng_query_interface,
    eng_addref,
    eng_release,
    eng_speak,
    eng_get_output_format,
    eng_get_status
};

static HRESULT STDMETHODCALLTYPE tok_query_interface(
    ISpObjectWithToken *self_, REFIID riid, void **out);
static ULONG STDMETHODCALLTYPE tok_addref(ISpObjectWithToken *self_);
static ULONG STDMETHODCALLTYPE tok_release(ISpObjectWithToken *self_);
static HRESULT STDMETHODCALLTYPE tok_set_object_token(ISpObjectWithToken *self_,
                                                      ISpObjectToken *token);
static HRESULT STDMETHODCALLTYPE tok_get_object_token(ISpObjectWithToken *self_,
                                                      ISpObjectToken **out);

static const ISpObjectWithTokenVtbl eng_token_vtbl = {
    tok_query_interface,
    tok_addref,
    tok_release,
    tok_set_object_token,
    tok_get_object_token
};

/* ---- process-wide start ------------------------------------------------ */

static BOOL CALLBACK engine_once_init(PINIT_ONCE once, PVOID param,
                                      PVOID *context)
{
    (void)once;
    (void)param;
    (void)context;
    evv_port_start();
    evvRunStaticInitialisers();
    return TRUE;
}

static void engine_ensure_started(void)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

    InitOnceExecuteOnce(&once, engine_once_init, NULL, NULL);
}

/* ---- helpers ------------------------------------------------------------ */

static char *wide_to_utf8(const wchar_t *w, size_t chars)
{
    int bytes;
    char *s;

    if (w == NULL || chars == 0)
        return NULL;
    bytes = WideCharToMultiByte(CP_UTF8, 0, w, (int)chars, NULL, 0,
                                NULL, NULL);
    if (bytes <= 0)
        return NULL;
    s = malloc((size_t)bytes + 1);
    if (s == NULL)
        return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, (int)chars, s, bytes, NULL, NULL);
    s[bytes] = '\0';
    return s;
}

static void fill_wfx(WAVEFORMATEX *w)
{
    w->wFormatTag      = WAVE_FORMAT_PCM;
    w->nChannels       = 1;
    w->nSamplesPerSec  = RATE;
    w->wBitsPerSample  = 16;
    w->nBlockAlign     = (WORD)(w->nChannels * w->wBitsPerSample / 8);
    w->nAvgBytesPerSec = w->nSamplesPerSec * w->nBlockAlign;
    w->cbSize          = 0;
}

/* Write takes the samples and nothing else. What format they are in was
   settled once, by GetOutputFormat, before the site was ever handed over;
   a WAVEFORMATEX in front of every buffer is read as samples and heard as a
   click each time one goes by. */
static HRESULT write_audio(ISpTTSEngineSite *site, const short *samples,
                           ULONG count, ULONGLONG *offset)
{
    ULONG written = 0;

    if (count == 0)
        return S_OK;
    if (count > FRAME)
        count = FRAME;

    if (site->lpVtbl->Write(site, samples, count * 2, &written) != S_OK)
        return E_FAIL;
    *offset += (ULONGLONG)count * 2;
    return S_OK;
}

static void fire_event(EvvEngine *e, ISpTTSEngineSite *site, WORD id,
                       ULONGLONG offset, WPARAM wParam, LPARAM lParam,
                       WORD lParamType)
{
    SPEVENT ev;

    memset(&ev, 0, sizeof ev);
    ev.eEventId = id;
    ev.elParamType = lParamType;
    ev.ulStreamNum = e->stream_num;
    ev.ullAudioStreamOffset = offset;
    ev.wParam = wParam;
    ev.lParam = lParam;
    site->lpVtbl->AddEvents(site, &ev, 1);
}

/* ---- the engine callback ------------------------------------------------ */

static enum ECICallbackReturn STDCALL on_message(OldInst *h,
                                                 enum ECIMessage msg,
                                                 long param, void *data)
{
    EvvEngine *e = data;

    (void)h;
    if (msg != eciWaveformBuffer)
        return eciDataProcessed;
    if (e->aborting)
        return eciDataAbort;
    if (write_audio(e->site, frame_buf, (ULONG)param, &e->audio_offset) != S_OK)
        return eciDataAbort;
    return eciDataProcessed;
}

/* ---- instance management ------------------------------------------------ */

static HRESULT engine_build(EvvEngine *e)
{
    /* Room for more languages than a build is ever given. Eight was the
       count of voices, not of languages, and with ten linked in it left the
       last two unreachable: a token naming one of them found no match and
       was quietly given the first language instead. */
    uint32_t langs[32];
    int n = (int)(sizeof langs / sizeof langs[0]);
    OldInst *h;

    if (e->h != NULL)
        return S_OK;

    engine_ensure_started();

    /* Zero means it worked, which is the engine's own convention and not
       COM's; the CLI reads it the same way round. */
    if (eo_getAvailableLanguages(langs, &n) != 0 || n < 1)
        return E_FAIL;

    /* The token said which language it is; a token that did not, or that
       names one this build no longer carries, gets the first there is. */
    {
        int32_t want = e->lang;
        int     i, found = 0;

        for (i = 0; i < n; i++)
            if ((int32_t)langs[i] == want)
                found = 1;
        if (!found)
            want = (int32_t)langs[0];
        h = eo_newEx(want);
    }
    if (h == NULL)
        return E_FAIL;

    /* A person's units have to be on before any voice setting is read or
       written in them; speed then means words a minute and pitch hertz. */
    ev_setParam(h, P_REAL_WORLD_UNITS, 1);

    if (e->voice > 0 && !vc_copyVoice(h, e->voice, 0)) {
        es_delete(h);
        return E_FAIL;
    }

    e->base_speed  = vc_getVoiceParam(h, 0, V_SPEED);
    e->base_pitch  = vc_getVoiceParam(h, 0, V_PITCH);
    e->base_volume = vc_getVoiceParam(h, 0, V_VOLUME);
    if (e->base_speed <= 0)
        e->base_speed = 180;
    if (e->base_pitch <= 0)
        e->base_pitch = 65;
    /* Not a percentage: what the voices actually carry is around 60260 out
       of 65535, and that is the number a SAPI percentage is taken against. */
    if (e->base_volume <= 0)
        e->base_volume = 60260;

    /* The callback first: the engine will not take a sample buffer until it
       has somewhere to report the samples to. */
    eo_registerCallback(h, (void *)on_message, e);
    if (!ev_setOutputBuffer(h, FRAME, frame_buf)) {
        es_delete(h);
        return E_FAIL;
    }

    e->h = h;
    return S_OK;
}

static void engine_drop(EvvEngine *e)
{
    if (e->h != NULL) {
        es_delete(e->h);
        e->h = NULL;
    }
}

/* ---- ISpTTSEngine -------------------------------------------------------- */

static inline EvvEngine *eng_of(ISpTTSEngine *self_)
{
    return (EvvEngine *)self_;
}

static HRESULT STDMETHODCALLTYPE eng_query_interface(ISpTTSEngine *self_,
                                                     REFIID riid, void **out)
{
    EvvEngine *e = eng_of(self_);

    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown))
        *out = &e->tts;
    else if (IsEqualIID(riid, &IID_ISpTTSEngine_))
        *out = &e->tts;
    else if (IsEqualIID(riid, &IID_ISpObjectWithToken_))
        *out = &e->with_token;
    else
        return E_NOINTERFACE;
    InterlockedIncrement(&e->refs);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE eng_addref(ISpTTSEngine *self_)
{
    EvvEngine *e = eng_of(self_);

    return (ULONG)InterlockedIncrement(&e->refs);
}

static ULONG STDMETHODCALLTYPE eng_release(ISpTTSEngine *self_)
{
    EvvEngine *e = eng_of(self_);
    LONG refs = InterlockedDecrement(&e->refs);

    if (refs == 0) {
        EnterCriticalSection(&e->lock);
        engine_drop(e);
        LeaveCriticalSection(&e->lock);
        if (e->token != NULL)
            e->token->lpVtbl->Release(e->token);
        DeleteCriticalSection(&e->lock);
        free(e);
    }
    return (ULONG)refs;
}

/* Queue every fragment, then synthesize once: the engine shapes an utterance
   as a whole, and speaking it in pieces would sound like pieces. */
static HRESULT STDMETHODCALLTYPE eng_speak(ISpTTSEngine *self_, DWORD flags,
                                           REFGUID fmtid,
                                           const WAVEFORMATEX *wfx,
                                           const SPVTEXTFRAG *frags,
                                           ISpTTSEngineSite *site)
{
    EvvEngine *e = eng_of(self_);
    const SPVTEXTFRAG *f;
    long rate = 0;
    USHORT volume = 100;
    HRESULT hr = S_OK;
    int queued = 0;

    (void)flags;
    (void)fmtid;
    (void)wfx;

    EnterCriticalSection(&e->lock);
    e->site = site;
    e->aborting = 0;
    e->audio_offset = 0;
    e->stream_num++;
    InterlockedExchange(&e->speaking, 1);

    site->lpVtbl->GetRate(site, &rate);
    site->lpVtbl->GetVolume(site, &volume);

    hr = engine_build(e);
    if (FAILED(hr))
        goto done;

    fire_event(e, site, SPEI_START_INPUT_STREAM, 0, 0, 0,
               SPET_LPARAM_IS_UNDEFINED);

    for (f = frags; f != NULL && !e->aborting; f = f->pNext) {
        switch (f->State.eAction) {
        case SPVA_Speak:
        case SPVA_SpellOut: {
            char *utf8 = wide_to_utf8(f->pTextStart, f->ulTextLen);
            double total, wpm, hz;
            int vol;

            if (utf8 == NULL) {
                hr = E_OUTOFMEMORY;
                goto done;
            }

            /* SAPI's rate runs from -10 to 10 and doubles for every ten of
               it; the same scale the voice settings use, so the two add
               before the mapping. Pitch moves half as fast again. */
            total = (double)rate + (double)f->State.RateAdj;
            if (total > 10.0)
                total = 10.0;
            if (total < -10.0)
                total = -10.0;
            wpm = (double)e->base_speed * pow(2.0, total / 10.0);
            if (wpm < 50.0)
                wpm = 50.0;
            if (wpm > 800.0)
                wpm = 800.0;

            total = (double)f->State.PitchAdj.MiddleAdj;
            if (total > 10.0)
                total = 10.0;
            if (total < -10.0)
                total = -10.0;
            hz = (double)e->base_pitch * pow(2.0, total / 20.0);
            if (hz < 40.0)
                hz = 40.0;
            if (hz > 400.0)
                hz = 400.0;

            /* SAPI's volume is a percentage. The engine's is not: V_VOLUME
               runs to 65535 and a voice sits around 60260 of it, so writing
               the percentage straight in asks for a hundred parts in sixty
               thousand -- silence, and rounded to exact zeros at that. It is
               a percentage OF the voice's own volume. */
            vol = (int)f->State.Volume;
            if (vol == 0)
                vol = (int)volume;
            if (vol < 0)
                vol = 0;
            if (vol > 100)
                vol = 100;
            vol = (int)((double)e->base_volume * (double)vol / 100.0 + 0.5);

            vc_setVoiceParam(e->h, 0, V_SPEED, (int32_t)(wpm + 0.5));
            vc_setVoiceParam(e->h, 0, V_PITCH, (int32_t)(hz + 0.5));
            vc_setVoiceParam(e->h, 0, V_VOLUME, vol);

            if (!et_addText(e->h, utf8)) {
                free(utf8);
                hr = E_FAIL;
                goto done;
            }
            free(utf8);
            queued = 1;
            break;
        }
        case SPVA_Silence: {
            /* The engine has no pause of its own to ask for, and a run of
               quiet samples is exact: write zeros at the same rate until the
               milliseconds are spent. */
            ULONG left = f->State.SilenceMSecs * RATE / 1000;
            static short quiet[FRAME];

            memset(quiet, 0, sizeof quiet);
            while (left > 0 && !e->aborting) {
                ULONG chunk = left > FRAME ? FRAME : left;

                if (write_audio(site, quiet, chunk,
                                &e->audio_offset) != S_OK) {
                    hr = E_FAIL;
                    goto done;
                }
                left -= chunk;
            }
            break;
        }
        case SPVA_Bookmark: {
            /* SAPI matches bookmarks by name; the string has to outlive the
               event, and SPET_LPARAM_IS_STRING is the type that tells it the
               event owns a CoTaskMemAlloc'd copy. */
            wchar_t *copy = CoTaskMemAlloc(((SIZE_T)f->ulTextLen + 1) *
                                           sizeof(wchar_t));

            if (copy != NULL) {
                memcpy(copy, f->pTextStart,
                       (size_t)f->ulTextLen * sizeof(wchar_t));
                copy[f->ulTextLen] = L'\0';
                fire_event(e, site, SPEI_TTS_BOOKMARK, e->audio_offset, 0,
                           (LPARAM)copy, SPET_LPARAM_IS_STRING);
            }
            break;
        }
        default:
            /* Pronounce, Section and unknown tags carry nothing this engine
               could do better than ignore. */
            break;
        }
    }

    if (queued && !e->aborting) {
        DWORD acts;

        if (!et_synthesize(e->h)) {
            hr = E_FAIL;
            goto done;
        }

        /* Nothing drains the engine's message queue by itself; asking whether
           it is still speaking is what pumps it, and the callbacks arrive
           inside that call. Between pumps is where SAPI's wishes are read. */
        while (eo_speaking(e->h)) {
            acts = site->lpVtbl->GetActions(site);
            if (acts & SPVES_ABORT) {
                e->aborting = 1;
                break;
            }
            if (acts & SPVES_SKIP) {
                SPVSKIPTYPE type;
                long items = 0;

                site->lpVtbl->GetSkipInfo(site, &type, &items);
                e->aborting = 1;
                site->lpVtbl->CompleteSkip(site, items > 0 ? items : 1);
                break;
            }
            Sleep(4);
        }

        /* An abort asked for between pumps only reaches the engine through
           the callback's answer, and the callback only runs while someone
           pumps; keep asking until it has wound down, but not forever. */
        {
            int waits = 0;

            while (eo_speaking(e->h) && waits++ < 2500)
                Sleep(4);
        }
        eo_synchronizeSynth(e->h);
    }

    fire_event(e, site, SPEI_END_INPUT_STREAM, e->audio_offset, 0, 0,
               SPET_LPARAM_IS_UNDEFINED);

done:
    InterlockedExchange(&e->speaking, 0);
    e->site = NULL;
    LeaveCriticalSection(&e->lock);
    return hr;
}

static HRESULT STDMETHODCALLTYPE eng_get_output_format(ISpTTSEngine *self_,
                                                       const GUID *tfid,
                                                       const WAVEFORMATEX *twfx,
                                                       GUID *ofid,
                                                       WAVEFORMATEX **owfx)
{
    WAVEFORMATEX *w;

    (void)self_;
    (void)tfid;
    (void)twfx;

    if (ofid == NULL || owfx == NULL)
        return E_POINTER;
    w = CoTaskMemAlloc(sizeof *w);
    if (w == NULL)
        return E_OUTOFMEMORY;
    fill_wfx(w);
    *ofid = SPDFID_WaveFormatEx_;
    *owfx = w;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE eng_get_status(ISpTTSEngine *self_,
                                                SPVOICESTATUS *status)
{
    EvvEngine *e = eng_of(self_);
    int busy;

    if (status == NULL)
        return E_POINTER;

    busy = InterlockedCompareExchange(&e->speaking, 0, 0) != 0;
    status->ulCompletedStreamNum = e->stream_num > 0 ? e->stream_num - 1 : 0;
    status->dwRunningState = busy ? SPRS_IS_SPEAKING : SPRS_DONE;
    status->ulCurrentStream = e->stream_num;
    status->ulInputWordPos = 0;
    status->ulInputWordLen = 0;
    status->ulInputSentLen = 0;
    status->lBookMarkID = 0;
    return S_OK;
}

/* ---- ISpObjectWithToken --------------------------------------------------- */

static inline EvvEngine *tok_of(ISpObjectWithToken *self_)
{
    return (EvvEngine *)((char *)self_ - offsetof(EvvEngine, with_token));
}

static HRESULT STDMETHODCALLTYPE tok_query_interface(
    ISpObjectWithToken *self_, REFIID riid, void **out)
{
    return eng_query_interface(&tok_of(self_)->tts, riid, out);
}

static ULONG STDMETHODCALLTYPE tok_addref(ISpObjectWithToken *self_)
{
    return eng_addref(&tok_of(self_)->tts);
}

static ULONG STDMETHODCALLTYPE tok_release(ISpObjectWithToken *self_)
{
    return eng_release(&tok_of(self_)->tts);
}

static HRESULT STDMETHODCALLTYPE tok_set_object_token(ISpObjectWithToken *self_,
                                                      ISpObjectToken *token)
{
    EvvEngine *e = tok_of(self_);
    ISpDataKey *key = NULL;
    HRESULT hr;

    if (token == NULL)
        return E_INVALIDARG;

    EnterCriticalSection(&e->lock);
    if (e->token != NULL)
        e->token->lpVtbl->Release(e->token);
    e->token = token;
    token->lpVtbl->AddRef(token);
    e->voice = 1;
    e->lang = 0;

    /* Which of the eight this token names, and which language, both live in
       its attributes, written there by whoever registered us. */
    hr = token->lpVtbl->OpenKey(token, L"Attributes", &key);
    if (hr == S_OK && key != NULL) {
        LPWSTR value = NULL;

        if (key->lpVtbl->GetStringValue(key, L"Voice", &value) == S_OK &&
            value != NULL) {
            int v = _wtoi(value);

            if (v >= 1 && v <= 8)
                e->voice = v;
            CoTaskMemFree(value);
        }
        value = NULL;
        /* The engine's own number for the language, not the LCID beside it:
           `Language' is there for Windows to choose a voice by and says
           nothing about which module answers. */
        if (key->lpVtbl->GetStringValue(key, L"EvvLang", &value) == S_OK &&
            value != NULL) {
            e->lang = (int32_t)wcstol(value, NULL, 16);
            CoTaskMemFree(value);
        }
        key->lpVtbl->Release(key);
    }
    LeaveCriticalSection(&e->lock);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE tok_get_object_token(ISpObjectWithToken *self_,
                                                      ISpObjectToken **out)
{
    EvvEngine *e = tok_of(self_);

    if (out == NULL)
        return E_POINTER;
    EnterCriticalSection(&e->lock);
    *out = e->token;
    if (*out != NULL)
        (*out)->lpVtbl->AddRef(*out);
    LeaveCriticalSection(&e->lock);
    return *out != NULL ? S_OK : S_FALSE;
}

/* ---- class factory --------------------------------------------------------- */

static HRESULT STDMETHODCALLTYPE factory_query_interface(
    IClassFactory *self_, REFIID riid, void **out)
{
    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IClassFactory)) {
        *out = self_;
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE factory_addref(IClassFactory *self_)
{
    (void)self_;
    return 1;
}

static ULONG STDMETHODCALLTYPE factory_release(IClassFactory *self_)
{
    (void)self_;
    return 1;
}

static HRESULT STDMETHODCALLTYPE factory_lock_server(IClassFactory *self_,
                                                     BOOL lock)
{
    (void)self_;
    (void)lock;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE factory_create_instance(IClassFactory *self_,
                                                         IUnknown *outer,
                                                         REFIID riid,
                                                         void **out)
{
    EvvEngine *e;
    HRESULT hr;

    (void)self_;
    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (outer != NULL)
        return CLASS_E_NOAGGREGATION;

    e = calloc(1, sizeof *e);
    if (e == NULL)
        return E_OUTOFMEMORY;

    e->tts.lpVtbl = &eng_tts_vtbl;
    e->with_token.lpVtbl = &eng_token_vtbl;
    e->refs = 1;
    e->voice = 1;
    InitializeCriticalSection(&e->lock);

    hr = eng_query_interface(&e->tts, riid, out);
    eng_release(&e->tts);
    if (FAILED(hr)) {
        DeleteCriticalSection(&e->lock);
        free(e);
    }
    return hr;
}

static const IClassFactoryVtbl factory_vtbl = {
    factory_query_interface,
    factory_addref,
    factory_release,
    factory_create_instance,
    factory_lock_server
};

static IClassFactory evv_factory = { &factory_vtbl };

/* ---- registration ------------------------------------------------------------ */

static HMODULE g_self;

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        g_self = inst;
    return TRUE;
}

static BOOL set_string(HKEY key, const wchar_t *name, const wchar_t *value)
{
    return RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
                          (DWORD)((wcslen(value) + 1) *
                                  sizeof(wchar_t))) == ERROR_SUCCESS;
}

/* One published voice: one of the eight, in one of the languages this build
   carries. The key name carries the tag rather than a running number, so a
   user's chosen voice survives a build with a different set of languages in
   it -- adding German must not turn their English voice into a Spanish one.
 *
 * There was one token per voice before, and its Language always said 409,
 * so a build with several languages in it published eight English voices
 * and no way to reach the rest. */
static BOOL register_voice(const delta_language *lang, int n)
{
    wchar_t path[160];
    wchar_t sub[192];
    wchar_t num[8];
    wchar_t id[16];
    wchar_t tag[32];
    wchar_t langname[64];
    wchar_t shown[128];
    HKEY key = NULL;
    HKEY attrs = NULL;
    const wchar_t *gender = evv_voices[n - 1].gender;
    BOOL ok;

    widen(lang->tag, tag, sizeof tag / sizeof tag[0]);
    widen(lang->name, langname, sizeof langname / sizeof langname[0]);

    /* What a person picks from: the voice and the language it speaks. */
    wsprintfW(shown, L"%s - %s", evv_voices[n - 1].name, langname);

    wsprintfW(path, L"%s\\%s%s.%d", TOKENS_KEY, VOICE_PREFIX, tag, n);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;

    ok = set_string(key, NULL, shown) &&
         set_string(key, L"CLSID", CLSID_STRING);

    wsprintfW(sub, L"%s\\Attributes", path);
    if (ok &&
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, sub, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &attrs, NULL) == ERROR_SUCCESS) {
        wsprintfW(num, L"%d", n);
        wsprintfW(id, L"%x", (unsigned)lang->id);
        ok = set_string(attrs, L"Language", lcid_of(lang->id)) &&
             set_string(attrs, L"Name", shown) &&
             set_string(attrs, L"Gender", gender) &&
             set_string(attrs, L"Age", L"Adult") &&
             set_string(attrs, L"Vendor", L"openevv") &&
             set_string(attrs, L"Voice", num) &&
             /* Ours, and read back by SetObjectToken: the engine's own
                number for the language, which the LCID above cannot say. */
             set_string(attrs, L"EvvLang", id);
        RegCloseKey(attrs);
    }
    RegCloseKey(key);
    return ok;
}

static BOOL register_clsid(void)
{
    wchar_t path[MAX_PATH];
    wchar_t sub[160];
    HKEY key = NULL;
    HKEY server = NULL;
    BOOL ok;

    if (g_self == NULL ||
        GetModuleFileNameW(g_self, path, MAX_PATH) == 0)
        return FALSE;

    wsprintfW(sub, L"SOFTWARE\\Classes\\CLSID\\%s", CLSID_STRING);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, sub, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;

    ok = set_string(key, NULL, L"Open Eloquence SAPI 5 Engine");

    wcscat_s(sub, sizeof sub / sizeof sub[0], L"\\InprocServer32");
    if (ok &&
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, sub, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &server, NULL) == ERROR_SUCCESS) {
        ok = set_string(server, NULL, path) &&
             set_string(server, L"ThreadingModel", L"Both");
        RegCloseKey(server);
    }
    RegCloseKey(key);
    return ok;
}

__declspec(dllexport)
HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
    int i;

    int l;

    if (!register_clsid())
        return SELFREG_E_CLASS;
    /* A module states its own number in another translation unit, which C
       will not take in an initialiser, so the table says nought until this
       has run. Everything inside the engine reaches a language through a
       call that binds first; reading delta_languages[] from out here does
       not, and without this every token was written with a language of
       nought -- the neutral LCID and an EvvLang of 0 -- so all eighty
       voices asked for the same language and got the first one. */
    delta_lang_bind_all();
    /* Every language linked into this build, times the eight voices each of
       them has. delta_languages[] is what the build was made with, so a
       library with one language in it publishes eight voices as before. */
    for (l = 0; delta_languages[l] != 0; l++)
        for (i = 1; i <= 8; i++)
            if (!register_voice(delta_languages[l], i))
                return SELFREG_E_CLASS;
    return S_OK;
}

__declspec(dllexport)
HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
    wchar_t path[160];
    wchar_t sub[160];
    wchar_t tag[32];
    int i, l;

    /* Same reason as in DllRegisterServer: the tags below are static but
       the walk is over the same table, so bind before reading it. */
    delta_lang_bind_all();

    wsprintfW(sub, L"SOFTWARE\\Classes\\CLSID\\%s", CLSID_STRING);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, sub);

    for (l = 0; delta_languages[l] != 0; l++) {
        widen(delta_languages[l]->tag, tag, sizeof tag / sizeof tag[0]);
        for (i = 1; i <= 8; i++) {
            wsprintfW(path, L"%s\\%s%s.%d", TOKENS_KEY, VOICE_PREFIX, tag, i);
            RegDeleteTreeW(HKEY_LOCAL_MACHINE, path);
        }
    }

    /* The eight an older build published under a running number. Taking
       them out here is what stops an upgrade leaving voices behind that
       name a CLSID no longer registered. */
    for (i = 1; i <= 8; i++) {
        wsprintfW(path, L"%s\\%s%d", TOKENS_KEY, VOICE_PREFIX, i);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, path);
    }
    return S_OK;
}

/* ---- the four entry points SAPI knows us by ---------------------------------- */

__declspec(dllexport)
HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID riid,
                                         void **out)
{
    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (!IsEqualCLSID(clsid, &CLSID_OpenEloquence))
        return CLASS_E_CLASSNOTAVAILABLE;
    return factory_query_interface(&evv_factory, riid, out);
}

__declspec(dllexport)
HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
    return S_OK;
}
