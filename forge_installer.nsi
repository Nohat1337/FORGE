; Forge Language Windows Installer
; Build with: makensis forge_installer.nsi

!define FORGE_VERSION "1.0.0"
!define PRODUCT_NAME "Forge Programming Language"
!define PRODUCT_PUBLISHER "Forge Team"
!define PRODUCT_WEB_SITE "https://github.com/forge-lang/forge"
!define INSTALLER_NAME "forge-${FORGE_VERSION}-windows-x64.exe"

Name "${PRODUCT_NAME}"
OutFile "${INSTALLER_NAME}"
InstallDir "$PROGRAMFILES64\Forge"
InstallDirRegKey HKLM "Software\Forge" "InstallDir"
RequestExecutionLevel admin
Unicode true

; Modern UI
!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Install page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\forge.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

; Uninstall pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; Components
Page components
!insertmacro MUI_PAGE_COMPONENTS

Section "Core Interpreter" SEC_CORE
    SectionIn RO
    SetOutPath "$INSTDIR\bin"
    File /r "..\build-release\forge.exe"
    
    SetOutPath "$INSTDIR\share\forge\examples"
    File /r "..\examples\*.fge"
    
    ; Add to PATH
    EnvVarUpdate $0 "PATH" "A" "$INSTDIR\bin" "HKLM"
SectionEnd

Section "Forge Studio IDE" SEC_IDE
    SetOutPath "$INSTDIR\bin"
    File "..\ide\build\forge-studio.exe"
    
    ; Start menu shortcuts
    CreateDirectory "$SMPROGRAMS\Forge"
    CreateShortcut "$SMPROGRAMS\Forge\Forge Studio.lnk" "$INSTDIR\bin\forge-studio.exe"
    CreateShortcut "$SMPROGRAMS\Forge\Forge REPL.lnk" "$INSTDIR\bin\forge.exe"
    CreateShortcut "$SMPROGRAMS\Forge\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; Desktop shortcut
    CreateShortcut "$DESKTOP\Forge Studio.lnk" "$INSTDIR\bin\forge-studio.exe"
SectionEnd

Section "Developer Tools (formatter, linter, debugger)" SEC_TOOLS
    SetOutPath "$INSTDIR\bin"
    File "..\build-release\tools\forge-format.exe"
    File "..\build-release\tools\forge-lint.exe"
    File "..\build-release\tools\forge-debug.exe"
SectionEnd

Section "Documentation" SEC_DOCS
    SetOutPath "$INSTDIR\docs"
    File /r "..\docs\*"
SectionEnd

; Uninstaller
Section "Uninstall"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Remove from PATH
    EnvVarUpdate $0 "PATH" "R" "$INSTDIR\bin" "HKLM"
    
    ; Remove start menu
    RMDir /r "$SMPROGRAMS\Forge"
    
    ; Remove desktop shortcut
    Delete "$DESKTOP\Forge Studio.lnk"
    
    ; Remove installed files
    RMDir /r "$INSTDIR"
SectionEnd

; Registry for version detection
WriteRegStr HKLM "Software\Forge" "InstallDir" "$INSTDIR"
WriteRegStr HKLM "Software\Forge" "Version" "${FORGE_VERSION}"
WriteUninstaller "$INSTDIR\Uninstall.exe"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "DisplayName" "${PRODUCT_NAME}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "UninstallString" "\"$INSTDIR\Uninstall.exe\""
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "DisplayVersion" "${FORGE_VERSION}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "Publisher" "${PRODUCT_PUBLISHER}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "NoModify" 1
WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Forge" "NoRepair" 1

Function .onInit
    ; Check for 64-bit Windows
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONERROR "This installer only supports 64-bit Windows."
        Abort
    ${EndIf}
FunctionEnd