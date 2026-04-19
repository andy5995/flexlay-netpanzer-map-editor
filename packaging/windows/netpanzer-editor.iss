#define AppName    "NetPanzer Map Editor"
#define AppExe     "netpanzer-editor.exe"
#define Publisher  "netPanzer Project"
#define AppURL     "https://github.com/netpanzer/flexlay-netpanzer-map-editor"
#define IssuesURL  "https://github.com/netpanzer/flexlay-netpanzer-map-editor/issues"

#ifndef VERSION
  #define VERSION "0.1"
#endif
#ifndef ARCH
  #define ARCH "x86_64"
#endif

[Setup]
AppName={#AppName}
AppVersion={#VERSION}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#IssuesURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Output goes to the repo root (script is in packaging/windows/)
OutputDir=..\..\
OutputBaseFilename=netpanzer-editor-{#VERSION}-windows-{#ARCH}-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Executable
Source: "..\..\_staging\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; Runtime DLLs collected by collect-dlls.sh
Source: "..\..\_staging\*.dll";     DestDir: "{app}"; Flags: ignoreversion
; Qt5 platform plugin — required for any Qt5 GUI app on Windows
Source: "..\..\_staging\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion
; Bundled autotile data
Source: "..\..\data\autotile\*"; DestDir: "{app}\data\autotile"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExe}"; \
  Description: "Launch {#AppName}"; \
  Flags: nowait postinstall skipifsilent
