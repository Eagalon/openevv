; Inno Setup script for the OpenEloquence SAPI5 engine.
;
; Builds installers for both word sizes when both DLLs are present in
; build\; comment out the 32-bit Files entry if only the 64-bit one was
; built. The engine is self-contained: one DLL per word size, no data
; files, no runtime dependencies beyond SAPI5 itself (ships with Windows).

#define AppName "OpenEloquence SAPI5"
#define AppVersion "1.0"
#define AppPublisher "openevv contributors"
#define AppURL "https://github.com/Mudb0y/openevv"

[Setup]
AppId={{370245F1-D510-4895-A2C4-4C296A2A8FD5}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={autopf}\OpenEloquence
DefaultGroupName=OpenEloquence
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=OpenEloquence-SAPI5-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
; 64-bit engine, registered by the Run entry below.
Source: "..\build\OpenEloquence.dll"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode
; 32-bit engine, for thirty-two bit hosts on a sixty-four bit Windows.
Source: "..\build\OpenEloquence32.dll"; DestDir: "{app}"; Flags: ignoreversion; Check: not Is64BitInstallMode

[Run]
Filename: "regsvr32"; Parameters: "/s ""{app}\OpenEloquence.dll"""; \
    Flags: runhidden waituntilterminated; Check: Is64BitInstallMode
Filename: "regsvr32"; Parameters: "/s ""{app}\OpenEloquence32.dll"""; \
    Flags: runhidden waituntilterminated; Check: not Is64BitInstallMode

[UninstallRun]
Filename: "regsvr32"; Parameters: "/s /u ""{app}\OpenEloquence.dll"""; \
    Flags: runhidden waituntilterminated; RunOnceId: "Unreg64"
Filename: "regsvr32"; Parameters: "/s /u ""{app}\OpenEloquence32.dll"""; \
    Flags: runhidden waituntilterminated; RunOnceId: "Unreg32"
