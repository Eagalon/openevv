; Inno Setup script for the OpenEloquence SAPI5 engine.
;
; Builds against whichever DLLs are in build\: the 64-bit one, the 32-bit one,
; or both. Nothing here has to be commented out when only one was built --
; `make sapi` alone is enough to get an installer.
;
;   ISCC packaging\openevv-sapi.iss
;   ISCC /DAppVersion=1.2 packaging\openevv-sapi.iss
;
; The engine is self-contained: one DLL per word size, no data files, no
; runtime dependencies beyond SAPI5 itself, which ships with Windows.

#define AppName "OpenEloquence SAPI5"
#ifndef AppVersion
  #define AppVersion "1.0"
#endif
#define AppPublisher "openevv contributors"
#define AppURL "https://github.com/Mudb0y/openevv"

#define Dll64 AddBackslash(SourcePath) + "..\build\OpenEloquence.dll"
#define Dll32 AddBackslash(SourcePath) + "..\build\OpenEloquence32.dll"
#define Have64 FileExists(Dll64)
#define Have32 FileExists(Dll32)

#if !Have64 && !Have32
  #error No engine in build\. Run `make sapi' (and `make sapi32') first.
#endif

[Setup]
AppId={{370245F1-D510-4895-A2C4-4C296A2A8FD5}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={autopf}\OpenEloquence
DefaultGroupName=OpenEloquence
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=OpenEloquence-SAPI5-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
; Registration writes to HKLM, so there is no unprivileged install to offer.
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
#if Have64
; The 64-bit engine, for 64-bit SAPI hosts. Only meaningful on 64-bit Windows.
Source: "{#Dll64}"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode
#endif
#if Have32
; The 32-bit engine. Not an either/or with the one above: a 32-bit program
; asking Windows for voices sees only 32-bit engines, so on a 64-bit machine
; both belong there. Installed unconditionally for that reason.
Source: "{#Dll32}"; DestDir: "{app}"; Flags: ignoreversion
#endif

[Run]
#if Have64
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\OpenEloquence.dll"""; \
    StatusMsg: "Registering the 64-bit engine..."; \
    Flags: runhidden waituntilterminated; Check: Is64BitInstallMode
#endif
#if Have32
; {syswow64} is the 32-bit regsvr32 on a 64-bit Windows and {sys} on a 32-bit
; one. The 64-bit regsvr32 refuses a 32-bit DLL outright, and registering
; through this one is also what puts the entries under Wow6432Node, which is
; where a 32-bit host looks for them.
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/s ""{app}\OpenEloquence32.dll"""; \
    StatusMsg: "Registering the 32-bit engine..."; \
    Flags: runhidden waituntilterminated; Check: Is64BitInstallMode
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\OpenEloquence32.dll"""; \
    StatusMsg: "Registering the 32-bit engine..."; \
    Flags: runhidden waituntilterminated; Check: not Is64BitInstallMode
#endif

[UninstallRun]
; Unregister before the files go, and by the same word size that registered.
#if Have64
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\OpenEloquence.dll"""; \
    Flags: runhidden waituntilterminated; RunOnceId: "Unreg64"; \
    Check: Is64BitInstallMode
#endif
#if Have32
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/s /u ""{app}\OpenEloquence32.dll"""; \
    Flags: runhidden waituntilterminated; RunOnceId: "Unreg32"; \
    Check: Is64BitInstallMode
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\OpenEloquence32.dll"""; \
    Flags: runhidden waituntilterminated; RunOnceId: "Unreg32w"; \
    Check: not Is64BitInstallMode
#endif
