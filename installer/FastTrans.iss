#define MyAppName "FastTrans"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "JustDuck"
#define MyAppExeName "FastTrans.exe"

[Setup]
AppId={{C7BE49E2-7E0E-4E8C-AB71-722D4E2D7B2D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\build\installer
OutputBaseFilename=FastTransSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\assets\app_icon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "launchonstartup"; Description: "Launch FastTrans when Windows starts"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
Source: "..\build\manual\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\tesseract\tesseract.exe"; DestDir: "{app}\third_party\tesseract"; Flags: ignoreversion
Source: "..\third_party\tesseract\tessdata\*.traineddata"; DestDir: "{app}\third_party\tesseract\tessdata"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: launchonstartup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
