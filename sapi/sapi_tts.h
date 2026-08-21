/* The pieces of SAPI 5 that a text-to-speech engine touches, written out in
 * plain C.
 *
 * The Windows SDK ships these same declarations in sapi.h and sapiddk.h, but
 * those are C++-first and pull in half of OLE. A wrapper this small is easier
 * to read against a page of its own than against two thousand lines of MIDL
 * output, and the layouts here are copied from the SDK headers verbatim: an
 * interface vtable is a contract, and this file keeps its side of it.
 */

#ifndef EVV_SAPI_TTS_H
#define EVV_SAPI_TTS_H

#include <windows.h>
#include <mmreg.h>
#include <objbase.h>
#include <unknwn.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- identifiers ------------------------------------------------------
 *
 * The interface ids are listed here rather than declared: whoever compiles
 * against this header defines the ones they need, which keeps one translation
 * unit owning the storage and spares this header an INITGUID dance. The
 * values are copied from the SDK headers.
 *
 *   IID_ISpTTSEngine      {A74D7C8E-4CC5-4F2F-A6EB-804DEE18500E}
 *   IID_ISpTTSEngineSite  {9880499B-CCE9-11D2-B503-00C04F797396}
 *   IID_ISpObjectWithToken {5B559F40-E952-11D2-BB91-00C04F8EE6C0}
 *   IID_ISpDataKey        {14056581-E16C-11D2-BB90-00C04F8EE6C0}
 *   SPDFID_WaveFormatEx   {C31ADBAE-527F-4FF5-A230-F62BB61FF70C}
 */

/* ---- forward declarations --------------------------------------------- */

typedef struct ISpDataKey        ISpDataKey;
typedef struct ISpObjectToken    ISpObjectToken;
typedef struct ISpTTSEngine      ISpTTSEngine;
typedef struct ISpTTSEngineSite  ISpTTSEngineSite;
typedef struct ISpObjectWithToken ISpObjectWithToken;

/* ---- speak flags (SPF_) ------------------------------------------------ */

#define SPF_DEFAULT           0
#define SPF_ASYNC             (1L << 0)
#define SPF_PURGEBEFORESPEAK  (1L << 1)
#define SPF_IS_FILENAME       (1L << 2)
#define SPF_IS_XML            (1L << 3)
#define SPF_IS_NOT_XML        (1L << 4)
#define SPF_PERSIST_XML       (1L << 5)
#define SPF_NLP_SPEAK_PUNC    (1L << 6)

/* ---- fragment actions (SPVACTIONS) ------------------------------------- */

typedef enum SPVACTIONS {
    SPVA_Speak          = 0,
    SPVA_Silence        = 1,
    SPVA_Pronounce      = 2,
    SPVA_Bookmark       = 3,
    SPVA_SpellOut       = 4,
    SPVA_Section        = 5,
    SPVA_ParseUnknownTag = 6
} SPVACTIONS;

typedef enum SPPARTOFSPEECH {
    SPOS_Verb       = 0,
    SPOS_Noun       = 1,
    SPOS_Modifier   = 2,
    SPOS_Function   = 3,
    SPOS_Interjection = 4,
    SPOS_Unknown    = 5,
    SPOS_SUPPRESS   = 6
} SPPARTOFSPEECH;

typedef struct SPVPITCH {
    long MiddleAdj;
    long RangeAdj;
} SPVPITCH;

typedef struct SPVCONTEXT {
    LPCWSTR pCategory;
    LPCWSTR pBefore;
    LPCWSTR pAfter;
} SPVCONTEXT;

/* The state SAPI has accumulated for one run of text. RateAdj and
   PitchAdj.MiddleAdj are relative to what the site reports through GetRate
   and GetVolume; Volume is absolute. */
typedef struct SPVSTATE {
    SPVACTIONS     eAction;
    LANGID         LangID;
    WORD           wReserved;
    long           EmphAdj;
    long           RateAdj;
    ULONG          Volume;
    SPVPITCH       PitchAdj;
    ULONG          SilenceMSecs;
    void          *pPhoneIds;
    SPPARTOFSPEECH ePartOfSpeech;
    SPVCONTEXT     Context;
} SPVSTATE;

typedef struct SPVTEXTFRAG {
    struct SPVTEXTFRAG *pNext;
    SPVSTATE            State;
    LPCWSTR             pTextStart;
    ULONG               ulTextLen;
    ULONG               ulTextSrcOffset;
} SPVTEXTFRAG;

/* ---- events ------------------------------------------------------------ */

typedef enum SPEVENTENUM {
    SPEI_UNDEFINED           = 0,
    SPEI_START_INPUT_STREAM  = 1,
    SPEI_END_INPUT_STREAM    = 2,
    SPEI_VOICE_CHANGE        = 3,
    SPEI_TTS_BOOKMARK        = 4,
    SPEI_WORD_BOUNDARY       = 5,
    SPEI_PHONEME             = 6,
    SPEI_SENTENCE_BOUNDARY   = 7,
    SPEI_VISEME              = 8,
    SPEI_TTS_PRIVATE         = 15
} SPEVENTENUM;

typedef enum SPEVENTLPARAMTYPE {
    SPET_LPARAM_IS_UNDEFINED = 0,
    SPET_LPARAM_IS_TOKEN     = 1,
    SPET_LPARAM_IS_OBJECT    = 2,
    SPET_LPARAM_IS_POINTER   = 3,
    SPET_LPARAM_IS_STRING    = 4
} SPEVENTLPARAMTYPE;

typedef struct SPEVENT {
    WORD          eEventId;
    WORD          elParamType;
    ULONG         ulStreamNum;
    ULONGLONG     ullAudioStreamOffset;
    WPARAM        wParam;
    LPARAM        lParam;
} SPEVENT;

typedef enum SPRUNSTATE {
    SPRS_DONE         = (1 << 0),
    SPRS_IS_SPEAKING  = (1 << 1)
} SPRUNSTATE;

typedef enum SPVESACTIONS {
    SPVES_CONTINUE = 0,
    SPVES_ABORT    = (1L << 0),
    SPVES_SKIP     = (1L << 1),
    SPVES_RATE     = (1L << 2),
    SPVES_VOLUME   = (1L << 3)
} SPVESACTIONS;

typedef enum SPVSKIPTYPE {
    SPVST_SENTENCE = (1L << 0)
} SPVSKIPTYPE;

/* What Speak's status argument carries back. */
typedef struct SPVOICESTATUS {
    ULONG    ulCompletedStreamNum;
    volatile HRESULT hrLastResult;
    DWORD    dwRunningState;
    ULONG    ulInputWordPos;
    ULONG    ulInputWordLen;
    ULONG    ulInputSentLen;
    LONG     lBookMarkID;
    PVOID    pvInputFragList;
    ULONG    ulCurrentStream;
} SPVOICESTATUS;

#define SPVOICESTATUS_OFFSET_STATUS   offsetof(SPVOICESTATUS, dwRunningState)

/* ---- ISpDataKey --------------------------------------------------------- */

#undef  INTERFACE
#define INTERFACE ISpDataKey
DECLARE_INTERFACE_(ISpDataKey, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /* ISpDataKey */
    STDMETHOD(SetData)(THIS_ LPCWSTR, ULONG, const BYTE *) PURE;
    STDMETHOD(GetData)(THIS_ LPCWSTR, ULONG *, BYTE *) PURE;
    STDMETHOD(SetStringValue)(THIS_ LPCWSTR, LPCWSTR) PURE;
    STDMETHOD(GetStringValue)(THIS_ LPCWSTR, LPWSTR *) PURE;
    STDMETHOD(SetDWORD)(THIS_ LPCWSTR, DWORD) PURE;
    STDMETHOD(GetDWORD)(THIS_ LPCWSTR, DWORD *) PURE;
    STDMETHOD(OpenKey)(THIS_ LPCWSTR, ISpDataKey **) PURE;
    STDMETHOD(CreateKey)(THIS_ LPCWSTR, ISpDataKey **) PURE;
    STDMETHOD(DeleteKey)(THIS_ LPCWSTR) PURE;
    STDMETHOD(DeleteValue)(THIS_ LPCWSTR) PURE;
    STDMETHOD(EnumKeys)(THIS_ ULONG, LPWSTR *) PURE;
    STDMETHOD(EnumValues)(THIS_ ULONG, LPWSTR *) PURE;
};

/* ---- ISpObjectToken ----------------------------------------------------- */

#undef  INTERFACE
#define INTERFACE ISpObjectToken
DECLARE_INTERFACE_(ISpObjectToken, ISpDataKey)
{
    /* ISpDataKey */
    STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD(SetData)(THIS_ LPCWSTR, ULONG, const BYTE *) PURE;
    STDMETHOD(GetData)(THIS_ LPCWSTR, ULONG *, BYTE *) PURE;
    STDMETHOD(SetStringValue)(THIS_ LPCWSTR, LPCWSTR) PURE;
    STDMETHOD(GetStringValue)(THIS_ LPCWSTR, LPWSTR *) PURE;
    STDMETHOD(SetDWORD)(THIS_ LPCWSTR, DWORD) PURE;
    STDMETHOD(GetDWORD)(THIS_ LPCWSTR, DWORD *) PURE;
    STDMETHOD(OpenKey)(THIS_ LPCWSTR, ISpDataKey **) PURE;
    STDMETHOD(CreateKey)(THIS_ LPCWSTR, ISpDataKey **) PURE;
    STDMETHOD(DeleteKey)(THIS_ LPCWSTR) PURE;
    STDMETHOD(DeleteValue)(THIS_ LPCWSTR) PURE;
    STDMETHOD(EnumKeys)(THIS_ ULONG, LPWSTR *) PURE;
    STDMETHOD(EnumValues)(THIS_ ULONG, LPWSTR *) PURE;
    /* ISpObjectToken */
    STDMETHOD(SetId)(THIS_ LPCWSTR, LPCWSTR, BOOL) PURE;
    STDMETHOD(GetId)(THIS_ LPWSTR *) PURE;
    STDMETHOD(GetCategory)(THIS_ struct ISpObjectTokenCategory **) PURE;
    STDMETHOD(CreateInstance)(THIS_ IUnknown *, DWORD, REFIID, void **) PURE;
    STDMETHOD(GetStorageFileName)(THIS_ REFCLSID, LPCWSTR, LPCWSTR, ULONG,
                                  LPWSTR *) PURE;
    STDMETHOD(RemoveStorageFileName)(THIS_ REFCLSID, LPCWSTR, BOOL) PURE;
    STDMETHOD(Remove)(THIS_ const CLSID *) PURE;
    STDMETHOD(IsUISupported)(THIS_ LPCWSTR, void *, ULONG, IUnknown *,
                             BOOL *) PURE;
    STDMETHOD(DisplayUI)(THIS_ HWND, LPCWSTR, LPCWSTR, void *, ULONG,
                         IUnknown *) PURE;
    STDMETHOD(MatchesAttributes)(THIS_ LPCWSTR, BOOL *) PURE;
};

/* ---- ISpTTSEngineSite ---------------------------------------------------- */

#undef  INTERFACE
#define INTERFACE ISpTTSEngineSite
DECLARE_INTERFACE_(ISpTTSEngineSite, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /* ISpEventSink */
    STDMETHOD(AddEvents)(THIS_ const SPEVENT *, ULONG) PURE;
    STDMETHOD(GetEventInterest)(THIS_ ULONGLONG *) PURE;
    /* ISpTTSEngineSite */
    STDMETHOD(GetActions)(THIS) PURE;
    STDMETHOD(Write)(THIS_ const void *, ULONG, ULONG *) PURE;
    STDMETHOD(GetRate)(THIS_ long *) PURE;
    STDMETHOD(GetVolume)(THIS_ USHORT *) PURE;
    STDMETHOD(GetSkipInfo)(THIS_ SPVSKIPTYPE *, long *) PURE;
    STDMETHOD(CompleteSkip)(THIS_ long) PURE;
};

/* ---- ISpTTSEngine -------------------------------------------------------- */

#undef  INTERFACE
#define INTERFACE ISpTTSEngine
DECLARE_INTERFACE_(ISpTTSEngine, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /* ISpTTSEngine */
    STDMETHOD(Speak)(THIS_ DWORD, REFGUID, const WAVEFORMATEX *,
                     const SPVTEXTFRAG *, ISpTTSEngineSite *) PURE;
    STDMETHOD(GetOutputFormat)(THIS_ const GUID *, const WAVEFORMATEX *,
                               GUID *, WAVEFORMATEX **) PURE;
    STDMETHOD(GetStatus)(THIS_ SPVOICESTATUS *) PURE;
};

/* ---- ISpObjectWithToken --------------------------------------------------- */

#undef  INTERFACE
#define INTERFACE ISpObjectWithToken
DECLARE_INTERFACE_(ISpObjectWithToken, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /* ISpObjectWithToken */
    STDMETHOD(SetObjectToken)(THIS_ ISpObjectToken *) PURE;
    STDMETHOD(GetObjectToken)(THIS_ ISpObjectToken **) PURE;
};

#ifdef __cplusplus
}
#endif

#endif /* EVV_SAPI_TTS_H */
