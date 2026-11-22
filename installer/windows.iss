#ifndef SourceRoot
  #error SourceRoot is required. Use scripts/build-windows.ps1.
#endif
#ifndef OutputRoot
  #error OutputRoot is required.
#endif
[Setup]
AppId={{594E184F-8D16-497A-B616-F29586FA6914}
AppName=LYB Live Vocal Beta
AppVersion=0.1.1
AppPublisher=LYB
DefaultDirName={autopf}\LYB\Live Vocal
DefaultGroupName=LYB Live Vocal
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputRoot}
OutputBaseFilename=LYB-Live-Vocal-0.1.1-Windows-x64-Beta-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile={#SourceRoot}\installer\BETA-NOTICE.txt
UninstallDisplayName=LYB Live Vocal Beta

[Files]
Source: "{#SourceRoot}\build\LYBLiveVocal_artefacts\Release\VST3\LYB Live Vocal.vst3\*"; DestDir: "{commoncf64}\VST3\LYB Live Vocal.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\build\LYBLiveVocal_artefacts\Release\Standalone\LYB Live Vocal.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceRoot}\installer\BETA-NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\LYB Live Vocal Beta"; Filename: "{app}\LYB Live Vocal.exe"
Name: "{group}\Uninstall LYB Live Vocal Beta"; Filename: "{uninstallexe}"
