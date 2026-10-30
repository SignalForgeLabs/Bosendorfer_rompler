; Inno Setup Script for SignalForgeLabs Bosendorfer Imperial 290
; Compatible with Inno Setup 6.x

#define MyAppName "Bosendorfer Imperial 290"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SignalForgeLabs"
#define MyAppURL "https://github.com/SignalForgeLabs/Bosendorfer_rompler"
#define MyAppExeName "Bosendorfer Imperial 290.exe"
#define MyPluginName "Bosendorfer Imperial 290.vst3"

[Setup]
AppId={{B290A97C-4F1E-4C2B-9E88-7D5F3A11B290}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\SignalForgeLabs\Bosendorfer Imperial 290
DefaultGroupName=SignalForgeLabs\Bosendorfer Imperial 290
DisableProgramGroupPage=no
OutputBaseFilename=Bosendorfer_Imperial_290_Windows_Setup_v{#MyAppVersion}
OutputDir=..\..\dist\windows
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
ChangesEnvironment=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full Installation (Recommended)"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 64-bit Plugin (Cubase, Ableton, FL Studio, Reaper, Studio One)"; Types: full custom; Flags: checkablealone
Name: "standalone"; Description: "Standalone Application (Run without a DAW)"; Types: full custom; Flags: checkablealone
Name: "docs"; Description: "Documentation & User Guide"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[Files]
; 1. VST3 Plugin (Steinberg 64-bit standard folder)
Source: "..\..\build\BosendorferImperial_artefacts\Release\VST3\{#MyPluginName}\*"; DestDir: "{commoncf}\VST3\{#MyPluginName}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: vst3; Check: Is64BitInstallMode
Source: "..\..\build\BosendorferImperial_artefacts\VST3\{#MyPluginName}\*"; DestDir: "{commoncf}\VST3\{#MyPluginName}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: vst3; Check: Is64BitInstallMode

; 2. Standalone Application
Source: "..\..\build\BosendorferImperial_artefacts\Release\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: standalone
Source: "..\..\build\BosendorferImperial_artefacts\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: standalone

; 3. Documentation
Source: "..\common\README_INSTALL.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: docs

[Icons]
; Start Menu Shortcuts
Name: "{group}\{#MyAppName} Standalone"; Filename: "{app}\{#MyAppExeName}"; Components: standalone
Name: "{group}\Documentation"; Filename: "{app}\README.txt"; Components: docs
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop Shortcut
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\{#MyPluginName}"
Type: filesandordirs; Name: "{app}"

