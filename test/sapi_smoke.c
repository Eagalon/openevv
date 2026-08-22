/* A stand-in SAPI host, small enough to read in one sitting.
 *
 * What SAPI does for real -- load the DLL off the registry, hand it tokens,
 * take its samples into a wave sink -- is done here by hand: the DLL is
 * loaded straight off its path, a fake token names the voice, and a fake
 * site collects every buffer Write delivers. What comes out is written as a
 * wave file, which is the same proof the CLI gives: samples that play are
 * samples the engine made.
 *
 * Usage: sapi_smoke.exe [-d dll] [-v 1..8] [-r -10..10] [-o out.wav]
 *                       [--abort-after n] [text ...]
 */

#include <windows.h>
#include <mmreg.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sapi/sapi_tts.h"

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
static const GUID IID_ISpDataKey_ = EVV_GUID(0x14056581, 0xe16c, 0x11d2,
    0xbb, 0x90, 0x00, 0xc0, 0x4f, 0x8e, 0xe6, 0xc0);
static const GUID IID_ISpObjectToken_ = EVV_GUID(0x14056589, 0xe16c, 0x11d2,
    0xbb, 0x90, 0x00, 0xc0, 0x4f, 0x8e, 0xe6, 0xc0);
static const GUID SPDFID_WaveFormatEx_ = EVV_GUID(0xc31adbae, 0x527f, 0x4ff5,
    0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);

#undef EVV_GUID

/* What a real data key answers for a value it does not have. The number is
   SAPI's own; the wrapper only ever tests against S_OK. */
#define SPERR_NOT_FOUND_ 0x8004503aL

#define RATE 11025

/* Speech off this engine peaks in the twenties of thousands; the first two
   thousand samples of test/dll.c's output already reach 15261. Anything
   under this is not quiet speech, it is no speech. */
#define QUIET_FLOOR 1000

/* The frame the wrapper hands the library, and so the most any one Write
   can carry. */
#define FRAME 2048

/* A crash here used to be a bare "Segmentation fault" and nothing else --
   gdb on this machine cannot enumerate modules, so a backtrace is question
   marks. This says which module faulted and how far into it, which is all
   that is needed to name the function with nm. */
static LONG CALLBACK say_where(EXCEPTION_POINTERS *ep)
{
    void *addr = ep->ExceptionRecord->ExceptionAddress;
    HMODULE mod = NULL;
    char name[MAX_PATH] = "?";

    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)addr, &mod);
    if (mod != NULL)
        GetModuleFileNameA(mod, name, sizeof name);
    fprintf(stderr, "\nsmoke: ACCESS VIOLATION %s 0x%p\n",
            ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
            (void *)ep->ExceptionRecord->ExceptionInformation[1]);
    fprintf(stderr, "smoke: at 0x%p in %s\n", addr, name);
    if (mod != NULL)
        fprintf(stderr, "smoke: that is +0x%llX into the module\n",
                (unsigned long long)((char *)addr - (char *)mod));
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- the fake site ------------------------------------------------------ */

typedef struct MockSite {
    ISpTTSEngineSite vt;
    LONG refs;
    unsigned char *audio;
    size_t bytes;
    size_t cap;
    long rate;
    int writes_before_abort;
    int bad_writes;
    int events_started;
    int events_ended;
} MockSite;

static HRESULT STDMETHODCALLTYPE site_qi(ISpTTSEngineSite *self_, REFIID riid,
                                         void **out)
{
    MockSite *s = (MockSite *)self_;

    (void)s;
    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_ISpTTSEngineSite_)) {
        *out = self_;
        self_->lpVtbl->AddRef(self_);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE site_addref(ISpTTSEngineSite *self_)
{
    MockSite *s = (MockSite *)self_;

    return (ULONG)InterlockedIncrement(&s->refs);
}

static ULONG STDMETHODCALLTYPE site_release(ISpTTSEngineSite *self_)
{
    MockSite *s = (MockSite *)self_;
    LONG refs = InterlockedDecrement(&s->refs);

    return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE site_add_events(ISpTTSEngineSite *self_,
                                                 const SPEVENT *events,
                                                 ULONG count)
{
    MockSite *s = (MockSite *)self_;
    ULONG i;

    for (i = 0; i < count; i++) {
        if (events[i].eEventId == SPEI_START_INPUT_STREAM)
            s->events_started++;
        if (events[i].eEventId == SPEI_END_INPUT_STREAM)
            s->events_ended++;
        if (events[i].elParamType == SPET_LPARAM_IS_STRING)
            CoTaskMemFree((void *)events[i].lParam);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE site_event_interest(ISpTTSEngineSite *self_,
                                                     ULONGLONG *mask)
{
    (void)self_;
    *mask = 0;
    return S_OK;
}

static DWORD STDMETHODCALLTYPE site_actions(ISpTTSEngineSite *self_)
{
    MockSite *s = (MockSite *)self_;

    return s->writes_before_abort == 0 ? SPVES_ABORT : SPVES_CONTINUE;
}

static HRESULT STDMETHODCALLTYPE site_write(ISpTTSEngineSite *self_,
                                            const void *buf, ULONG cb,
                                            ULONG *written)
{
    MockSite *s = (MockSite *)self_;

    /* What arrives is samples, all of it. A site that skipped a header here
       would agree with an engine that wrote one and the pair would sound
       fine to each other and to nothing else; that is exactly the bug this
       harness failed to see.

       So say what a buffer of samples has to look like. Whole samples, and
       never more than the frame the engine asked the library for: anything
       bolted on in front -- a WAVEFORMATEX is eighteen bytes -- pushes the
       count past the frame and is caught here rather than in someone's
       ears. Peak alone would not find it; the real samples are still loud. */
    if (cb % 2 != 0 || cb > (ULONG)FRAME * 2) {
        fprintf(stderr, "smoke: Write of %lu bytes is not %d whole samples or "
                        "fewer -- something is riding along with the audio\n",
                (unsigned long)cb, FRAME);
        s->bad_writes++;
    }
    if (cb > 0) {
        if (s->bytes + cb > s->cap) {
            s->cap = (s->bytes + cb) * 2 + 65536;
            s->audio = realloc(s->audio, s->cap);
            if (s->audio == NULL) {
                fprintf(stderr, "smoke: out of memory\n");
                exit(1);
            }
        }
        memcpy(s->audio + s->bytes, buf, cb);
        s->bytes += cb;
    }
    if (s->writes_before_abort > 0)
        s->writes_before_abort--;
    *written = cb;
    return S_OK;
}

/* The loudest sample in a stretch of the collected audio. This is the whole
   difference between "the site was written to" and "the engine spoke": a run
   that reports tens of thousands of bytes and a peak of nought delivered
   digital silence, which is what this harness used to call a pass. */
static int peak_of(const unsigned char *p, size_t bytes)
{
    size_t i;
    int peak = 0;

    for (i = 0; i + 1 < bytes; i += 2) {
        int v = (int)(short)((unsigned)p[i] | ((unsigned)p[i + 1] << 8));

        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    return peak;
}

static HRESULT STDMETHODCALLTYPE site_rate(ISpTTSEngineSite *self_, long *rate)
{
    MockSite *s = (MockSite *)self_;

    *rate = s->rate;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE site_volume(ISpTTSEngineSite *self_,
                                             USHORT *volume)
{
    (void)self_;
    *volume = 100;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE site_skip_info(ISpTTSEngineSite *self_,
                                                SPVSKIPTYPE *type,
                                                long *items)
{
    (void)self_;
    *type = SPVST_SENTENCE;
    *items = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE site_complete_skip(ISpTTSEngineSite *self_,
                                                    long items)
{
    (void)self_;
    (void)items;
    return S_OK;
}

static ISpTTSEngineSiteVtbl site_vtbl = {
    site_qi,
    site_addref,
    site_release,
    site_add_events,
    site_event_interest,
    site_actions,
    site_write,
    site_rate,
    site_volume,
    site_skip_info,
    site_complete_skip
};

/* ---- the fake token ------------------------------------------------------ */

/* One key that answers two questions: what subkeys are there, and what does
   a string value say. The Attributes key of a real token is asked nothing
   else by the wrapper. */

typedef struct FakeKey {
    ISpDataKey vt;
    LONG refs;
    const wchar_t *voice;
} FakeKey;

static HRESULT STDMETHODCALLTYPE key_qi(ISpDataKey *self_, REFIID riid,
                                        void **out)
{
    FakeKey *k = (FakeKey *)self_;

    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISpDataKey_)) {
        *out = self_;
        InterlockedIncrement(&k->refs);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE key_addref(ISpDataKey *self_)
{
    FakeKey *k = (FakeKey *)self_;

    return (ULONG)InterlockedIncrement(&k->refs);
}

static ULONG STDMETHODCALLTYPE key_release(ISpDataKey *self_)
{
    FakeKey *k = (FakeKey *)self_;
    LONG refs = InterlockedDecrement(&k->refs);

    if (refs == 0)
        free(k);
    return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE key_no(HRESULT hr)
{
    (void)hr;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE key_get_string(ISpDataKey *self_,
                                                LPCWSTR name, LPWSTR *value)
{
    FakeKey *k = (FakeKey *)self_;
    const wchar_t *answer = NULL;
    SIZE_T bytes;

    if (wcscmp(name, L"Voice") == 0)
        answer = k->voice;
    else if (wcscmp(name, L"Language") == 0)
        answer = L"409";
    if (answer == NULL)
        return SPERR_NOT_FOUND_;
    bytes = (wcslen(answer) + 1) * sizeof(wchar_t);
    *value = CoTaskMemAlloc(bytes);
    if (*value == NULL)
        return E_OUTOFMEMORY;
    memcpy(*value, answer, bytes);
    return S_OK;
}

static ISpDataKeyVtbl key_vtbl = {
    key_qi,
    key_addref,
    key_release,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR, ULONG,
                                  const BYTE *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR, ULONG *,
                                  BYTE *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR,
                                  LPCWSTR))key_no,
    key_get_string,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR, DWORD))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR, DWORD *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR,
                                  ISpDataKey **))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR,
                                  ISpDataKey **))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, LPCWSTR))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, ULONG, LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpDataKey *, ULONG, LPWSTR *))key_no
};

typedef struct FakeToken {
    ISpObjectToken vt;
    LONG refs;
    FakeKey root;
    FakeKey attrs;
} FakeToken;

static HRESULT STDMETHODCALLTYPE token_qi(ISpObjectToken *self_, REFIID riid,
                                          void **out)
{
    FakeToken *t = (FakeToken *)self_;

    if (out == NULL)
        return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISpDataKey_)) {
        *out = &t->root.vt;
        InterlockedIncrement(&t->root.refs);
        return S_OK;
    }
    if (IsEqualIID(riid, &IID_ISpObjectToken_)) {
        *out = self_;
        InterlockedIncrement(&t->attrs.refs);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE token_addref(ISpObjectToken *self_)
{
    FakeToken *t = (FakeToken *)self_;

    InterlockedIncrement(&t->root.refs);
    return (ULONG)InterlockedIncrement(&t->attrs.refs);
}

static ULONG STDMETHODCALLTYPE token_release(ISpObjectToken *self_)
{
    FakeToken *t = (FakeToken *)self_;
    LONG refs;

    InterlockedDecrement(&t->root.refs);
    refs = InterlockedDecrement(&t->attrs.refs);
    if (refs == 0)
        free(t);
    return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE token_open_key(ISpObjectToken *self_,
                                                LPCWSTR name,
                                                ISpDataKey **sub)
{
    FakeToken *t = (FakeToken *)self_;

    if (wcscmp(name, L"Attributes") != 0)
        return SPERR_NOT_FOUND_;
    *sub = &t->attrs.vt;
    InterlockedIncrement(&t->attrs.refs);
    return S_OK;
}

/* The token's own vtable: everything above ISpDataKey is answered with
   E_NOTIMPL, because the wrapper asks for none of it. */
static ISpObjectTokenVtbl token_vtbl = {
    token_qi,
    token_addref,
    token_release,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, ULONG,
                                  const BYTE *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, ULONG *,
                                  BYTE *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR,
                                  LPCWSTR))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR,
                                  LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, DWORD *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, DWORD *))key_no,
    token_open_key,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR,
                                  ISpDataKey **))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, ULONG, LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, ULONG, LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, LPCWSTR,
                                  BOOL))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *,
                                  struct ISpObjectTokenCategory **))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, IUnknown *, DWORD,
                                  REFIID, void **))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, REFCLSID, LPCWSTR,
                                  LPCWSTR, ULONG, LPWSTR *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, REFCLSID, LPCWSTR,
                                  BOOL))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *,
                                  const CLSID *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, void *, ULONG,
                                  IUnknown *, BOOL *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, HWND, LPCWSTR, LPCWSTR,
                                  void *, ULONG, IUnknown *))key_no,
    (HRESULT(STDMETHODCALLTYPE *)(ISpObjectToken *, LPCWSTR, BOOL *))key_no
};

static FakeToken *make_token(int voice)
{
    FakeToken *t = calloc(1, sizeof *t);
    wchar_t num[8];

    if (t == NULL) {
        fprintf(stderr, "smoke: out of memory\n");
        exit(1);
    }
    t->vt.lpVtbl = &token_vtbl;
    t->root.vt.lpVtbl = &key_vtbl;
    t->attrs.vt.lpVtbl = &key_vtbl;
    t->root.refs = 1;
    t->attrs.refs = 1;
    wsprintfW(num, L"%d", voice);
    {
        SIZE_T bytes = (wcslen(num) + 1) * sizeof(wchar_t);

        t->attrs.voice = malloc(bytes);
        wcscpy((wchar_t *)t->attrs.voice, num);
    }
    return t;
}

/* ---- wave writing --------------------------------------------------------- */

static void put32(FILE *f, unsigned long v)
{
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
    fputc((int)((v >> 16) & 0xff), f);
    fputc((int)((v >> 24) & 0xff), f);
}

static void put16(FILE *f, unsigned v)
{
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
}

static int write_wav(const char *path, const unsigned char *samples,
                     size_t bytes)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL)
        return 0;
    fwrite("RIFF", 1, 4, f);
    put32(f, (unsigned long)(36 + bytes));
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 1);
    put32(f, RATE);
    put32(f, RATE * 2);
    put16(f, 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, (unsigned long)bytes);
    fwrite(samples, 1, bytes, f);
    fclose(f);
    return 1;
}

/* ---- the drive ------------------------------------------------------------- */

typedef HRESULT (STDAPICALLTYPE *get_class_object_fn)(REFCLSID, REFIID,
                                                      void **);

int main(int argc, char **argv)
{
    const char *dll = "build/OpenEloquence.dll";
    const char *out = "build/smoke.wav";
    const wchar_t *text =
        L"The quick brown fox jumps over the lazy dog.";
    int voice = 1;
    long rate = 0;
    int abort_after = -1;
    int stress = 0;
    HMODULE lib;
    get_class_object_fn get_class_object;
    IClassFactory *factory = NULL;
    ISpTTSEngine *engine = NULL;
    ISpObjectWithToken *with_token = NULL;
    ISpTTSEngineSite *site;
    MockSite *ms;
    FakeToken *token;
    SPVTEXTFRAG frag;
    GUID fmtid;
    HRESULT hr;
    int i, runs;
    int failed = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dll = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc)
            voice = atoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            rate = atol(argv[++i]);
        else if (strcmp(argv[i], "--abort-after") == 0 && i + 1 < argc)
            abort_after = atoi(argv[++i]);
        else if (strcmp(argv[i], "--stress") == 0 && i + 1 < argc)
            stress = atoi(argv[++i]);
        else if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) {
            size_t chars;
            wchar_t *w;

            chars = MultiByteToWideChar(CP_UTF8, 0, argv[++i], -1, NULL, 0);
            w = malloc(chars * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, w, (int)chars);
            text = w;
        }
    }
    if (voice < 1 || voice > 8) {
        fprintf(stderr, "smoke: voice is 1 to 8\n");
        return 2;
    }

    lib = LoadLibraryA(dll);
    if (lib == NULL) {
        fprintf(stderr, "smoke: cannot load %s (error %lu)\n", dll,
                GetLastError());
        return 1;
    }
    get_class_object =
        (get_class_object_fn)(void *)GetProcAddress(lib, "DllGetClassObject");
    if (get_class_object == NULL) {
        fprintf(stderr, "smoke: the DLL has no DllGetClassObject\n");
        return 1;
    }

    hr = get_class_object(&CLSID_OpenEloquence, &IID_IClassFactory,
                          (void **)&factory);
    if (hr != S_OK) {
        fprintf(stderr, "smoke: no class factory (0x%08lx)\n",
                (unsigned long)hr);
        return 1;
    }
    hr = factory->lpVtbl->CreateInstance(factory, NULL, &IID_ISpTTSEngine_,
                                         (void **)&engine);
    factory->lpVtbl->Release(factory);
    if (hr != S_OK) {
        fprintf(stderr, "smoke: no engine (0x%08lx)\n", (unsigned long)hr);
        return 1;
    }

    AddVectoredExceptionHandler(1, say_where);

    ms = calloc(1, sizeof *ms);
    if (ms == NULL)
        return 1;
    ms->vt.lpVtbl = &site_vtbl;
    ms->refs = 1;
    ms->rate = rate;
    ms->writes_before_abort = abort_after;
    site = &ms->vt;

    token = make_token(voice);
    hr = engine->lpVtbl->QueryInterface(engine, &IID_ISpObjectWithToken_,
                                        (void **)&with_token);
    if (hr == S_OK) {
        hr = with_token->lpVtbl->SetObjectToken(with_token,
                                                &token->vt);
        if (hr != S_OK) {
            fprintf(stderr, "smoke: SetObjectToken refused (0x%08lx)\n",
                    (unsigned long)hr);
            failed = 1;
        }
        with_token->lpVtbl->Release(with_token);
    }

    memset(&frag, 0, sizeof frag);
    frag.State.eAction = SPVA_Speak;
    frag.State.Volume = 100;
    frag.pTextStart = text;
    frag.ulTextLen = (ULONG)wcslen(text);
    fmtid = SPDFID_WaveFormatEx_;

    /* What a screen reader does: speak, cut it off part way, speak again,
       hundreds of times on the one engine, cutting at a different point
       each time so the stop lands in a different place in the engine's
       work. Two runs never reached the race; this is the shape that does.
       Audio is not checked here -- most runs are meant to be cut short --
       only that the engine is still alive and answering. */
    if (stress > 0) {
        int i;

        for (i = 0; i < stress; i++) {
            ms->bytes = 0;
            ms->writes_before_abort = (i % 7 == 0) ? -1 : (i % 13) + 1;
            hr = engine->lpVtbl->Speak(engine, SPF_DEFAULT, &fmtid, NULL,
                                       &frag, site);
            if (hr != S_OK) {
                fprintf(stderr, "smoke: stress run %d refused (0x%08lx)\n",
                        i + 1, (unsigned long)hr);
                failed = 1;
                break;
            }
            if ((i + 1) % 25 == 0) {
                printf("stress: %d runs\n", i + 1);
                fflush(stdout);
            }
        }
        if (!failed)
            printf("stress: %d runs, engine still answering\n", stress);
        site->lpVtbl->Release(site);
        token->vt.lpVtbl->Release(&token->vt);
        engine->lpVtbl->Release(engine);
        FreeLibrary(lib);
        free(ms->audio);
        free(ms);
        return failed ? 1 : 0;
    }

    /* Two runs on the one engine: the second says whether an instance that
       has already spoken can be asked again, which is how SAPI uses it. */
    for (runs = 0; runs < 2 && !failed; runs++) {
        size_t before = ms->bytes;
        int peak;

        /* Only the first run aborts. Left latched at nought the site would
           abort every run after it on its first pump, and the second run --
           the one that asks whether an aborted engine still speaks, which is
           the whole point of the abort case -- could never say anything. */
        ms->writes_before_abort = runs == 0 ? abort_after : -1;

        hr = engine->lpVtbl->Speak(engine, SPF_DEFAULT, &fmtid, NULL,
                                   &frag, site);
        if (hr != S_OK) {
            fprintf(stderr, "smoke: Speak refused (0x%08lx)\n",
                    (unsigned long)hr);
            failed = 1;
            break;
        }
        if (ms->bytes == before) {
            fprintf(stderr, "smoke: run %d said nothing\n", runs + 1);
            failed = 1;
            break;
        }
        peak = peak_of(ms->audio + before, ms->bytes - before);
        printf("run %d: %lu bytes of audio, peak %d\n", runs + 1,
               (unsigned long)(ms->bytes - before), peak);
        if (peak < QUIET_FLOOR) {
            fprintf(stderr, "smoke: run %d is silence -- peak %d, and speech "
                            "reaches thousands. Bytes arriving is not audio "
                            "arriving.\n", runs + 1, peak);
            failed = 1;
            break;
        }
    }

    if (!failed && !write_wav(out, ms->audio, ms->bytes)) {
        fprintf(stderr, "smoke: cannot write %s\n", out);
        failed = 1;
    }
    if (!failed && ms->bad_writes != 0) {
        fprintf(stderr, "smoke: %d writes were not plain samples\n",
                ms->bad_writes);
        failed = 1;
    }
    if (!failed && (ms->events_started != runs || ms->events_ended != runs)) {
        fprintf(stderr, "smoke: stream events wrong (%d started, %d ended, "
                        "%d runs)\n", ms->events_started, ms->events_ended,
                runs);
        failed = 1;
    }
    if (!failed)
        printf("smoke: wrote %s\n", out);

    site->lpVtbl->Release(site);
    token->vt.lpVtbl->Release(&token->vt);
    engine->lpVtbl->Release(engine);
    FreeLibrary(lib);
    free(ms->audio);
    free(ms);
    return failed ? 1 : 0;
}
