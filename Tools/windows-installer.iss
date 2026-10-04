; The Windows installer: PedalCues.exe -> Program Files\PedalCues, the VST3 -> Common Files\VST3.
;
;   ISCC /DMyAppVersion=0.8.3 /DSourceDir=<folder with PedalCues.exe and PedalCues.vst3> /O<output folder> Tools\windows-installer.iss
;
; The fixed AppId makes a newer installer replace the installed version. Windows' Restart Manager finds a running
; PedalCues or a DAW that has the plugin loaded and asks to close it. Not code-signed, so SmartScreen asks once
; ("More info > Run anyway").

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\PedalCues-Windows"
#endif

[Setup]
AppId={{2CF05027-6517-4C57-A54D-A619D8B5CCAC}
AppName=PedalCues
AppVersion={#MyAppVersion}
AppVerName=PedalCues {#MyAppVersion}
AppPublisher=Thanasis Kostopoulos
AppPublisherURL=https://thankost.github.io/pedal-cues/
AppSupportURL=https://thankost.github.io/pedal-cues/help.html
AppUpdatesURL=https://github.com/thankost/pedal-cues/releases/latest
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\PedalCues
DefaultGroupName=PedalCues
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=PedalCues-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\PedalCues.exe
UninstallDisplayName=PedalCues

[Messages]
WelcomeLabel2=This installs PedalCues {#MyAppVersion}: the VST3 plugin and the standalone app.%n%nQuit your DAW first: a DAW that has PedalCues loaded keeps using the old version until it restarts. If PedalCues or your DAW is still open, Setup asks to close it.%n%nAn older PedalCues is replaced; your projects and settings are kept.

[Types]
Name: "full"; Description: "Plugin and standalone app"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3)"; Types: full custom
Name: "app"; Description: "Standalone app"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the app"; Components: app; Flags: unchecked

[InstallDelete]
; A clean plugin bundle: no files left over from an older version (or from copying the zip by hand).
Type: filesandordirs; Name: "{commoncf64}\VST3\PedalCues.vst3"; Components: vst3

[Files]
Source: "{#SourceDir}\PedalCues.vst3\*"; DestDir: "{commoncf64}\VST3\PedalCues.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\PedalCues.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{group}\PedalCues"; Filename: "{app}\PedalCues.exe"; Components: app
Name: "{autodesktop}\PedalCues"; Filename: "{app}\PedalCues.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PedalCues.exe"; Description: "Open PedalCues"; Components: app; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\PedalCues.vst3"
