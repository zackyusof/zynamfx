; Inno Setup script for the zynamfx Windows installer.
; Compiled with ISCC (preinstalled on GitHub windows-latest runners):
;
;   iscc /DVersion=x.y.z /DArtefactsDir=..\..\..\build\plugin\TONE3000_artefacts\Release script\installer\windows\zynamfx.iss
;
; Defines (override with /D on the command line):
;   Version       plugin version string, REQUIRED. Pass the contents of the
;                 repo-root VERSION file.
;   ArtefactsDir  path to the JUCE Release artefacts dir, absolute or relative
;                 to this script (default ..\..\..\build\plugin\TONE3000_artefacts\Release)
;   OutputDir     where the setup exe is written (default ..\..\..\release)

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from the repo-root VERSION file)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\..\build\plugin\TONE3000_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\release"
#endif

#define AppName "zynamfx"
#define Vst3Bundle ArtefactsDir + "\VST3\zynamfx.vst3"
#define StandaloneExe ArtefactsDir + "\Standalone\zynamfx.exe"
#define ClapFile ArtefactsDir + "\CLAP\zynamfx.clap"
#define AaxBundle ArtefactsDir + "\AAX\zynamfx.aaxplugin"

#if DirExists(AaxBundle)
  #define HaveAax
#endif
#if FileExists(ClapFile)
  #define HaveClap
#endif

[Setup]
AppId={{5D4E3C2B-1A09-4F8E-B7C6-9E8D7C6B5A4F}
AppName={#AppName}
AppVersion={#Version}
AppPublisher=ZYUSOF
AppPublisherURL=https://github.com/zackyusof/zynamfx
AppSupportURL=https://github.com/zackyusof/zynamfx/issues
AppUpdatesURL=https://github.com/zackyusof/zynamfx/releases
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\{#AppName}
DefaultGroupName={#AppName}
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-v{#Version}-windows-x64-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
InfoBeforeFile=..\..\..\LICENSE
WizardStyle=modern
DisableWelcomePage=no
WizardImageFile=wizard-image-100.bmp,wizard-image-200.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small-200.bmp
SetupIconFile=tone3000.ico
UninstallDisplayIcon={app}\{#AppName}.exe
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
ShowLanguageDialog=auto

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application (with ASIO support)"; Types: full custom
Name: "vst3";       Description: "VST3 plug-in (DAW effect)";                   Types: full custom
#ifdef HaveClap
Name: "clap";       Description: "CLAP plug-in";                               Types: full custom
#endif
#ifdef HaveAax
Name: "aax";        Description: "AAX plug-in (Pro Tools)";                    Types: full custom
#endif
Name: "presets";    Description: "Factory presets";                            Types: full custom

[Files]
; Standalone app -> Program Files\zynamfx
Source: "{#StandaloneExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; VST3 bundle -> Common Files\VST3 (standard Windows VST3 location)
Source: "{#Vst3Bundle}\*"; DestDir: "{commoncf64}\VST3\zynamfx.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#ifdef HaveClap
; CLAP -> Common Files\CLAP
Source: "{#ClapFile}"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
#endif
#ifdef HaveAax
; AAX bundle -> Common Files\Avid\Audio\Plug-Ins
Source: "{#AaxBundle}\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\zynamfx.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
; Factory presets -> ProgramData\zynamfx\Presets\Factory
Source: "..\..\..\resources\factory-presets\*.t3kpreset"; DestDir: "{commonappdata}\zynamfx\Presets\Factory"; Components: presets; Flags: ignoreversion

[InstallDelete]
Type: files; Name: "{commonappdata}\zynamfx\Presets\Factory\*.t3kpreset"; Components: presets

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Components: standalone

[Run]
Filename: "{app}\{#AppName}.exe"; Description: "Launch {#AppName}"; Components: standalone; Flags: nowait postinstall skipifsilent
