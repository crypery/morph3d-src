; =====================================================================
;  InstallMorph3D.iss - Inno Setup script for Morph3D screensaver
;  Устанавливает morph3d.scr в папку скринсэйверов Windows (System32)
;  и ставит его скринсэйвером по умолчанию. Требует прав администратора.
; =====================================================================

#define MyAppName "Morph3D"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Morph3D"
#define MyScrName "morph3d.scr"

[Setup]
AppId={{C9E6A7B2-4D1F-4E8A-9B3C-7F2D5E1A6C4B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=InstallMorph3D
SetupIconFile=icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Установка в System32 и в HKLM требует прав администратора -> UAC-запрос
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
; 64-битный скринсэйвер -> только 64-битные Windows, установка в System32
ArchitecturesAllowed=x64compatible arm64
ArchitecturesInstallIn64BitMode=x64compatible arm64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "setscreensaver"; Description: "Установить как скринсэйвер по умолчанию"; GroupDescription: "Скринсэйвер:"; Flags: checkedonce

[Files]
; Скринсэйвер в папку системных скринсэйверов Windows
Source: "morph3d.scr"; DestDir: "{sys}"; Flags: ignoreversion

[Registry]
; Скринсэйвер по умолчанию (HKLM\Control Panel\Desktop)
Root: HKLM; Subkey: "Control Panel\Desktop"; ValueType: string; ValueName: "ScreenSaver"; ValueData: "{#MyScrName}"; Tasks: setscreensaver
Root: HKLM; Subkey: "Control Panel\Desktop"; ValueType: string; ValueName: "SCRNSAVE.EXE"; ValueData: "{sys}\{#MyScrName}"; Tasks: setscreensaver

[UninstallDelete]
Type: filesandordirs; Name: "{sys}\{#MyScrName}"

[Run]
Filename: "ms-settings:screensaver"; Description: "Открыть настройки экрана"; Flags: postinstall nowait
