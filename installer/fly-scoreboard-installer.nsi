; ------------------------------------------------------------------------
; Fly Scoreboard – Windows Installer (NSIS)
; ------------------------------------------------------------------------
; Expects these defines from makensis:
;   /DPRODUCT_NAME
;   /DPRODUCT_VERSION
;   /DPROJECT_ROOT
;   /DCONFIGURATION
;   /DTARGET
;   /DOUTPUT_EXE
; ------------------------------------------------------------------------

Unicode true

!include "MUI2.nsh"
!include "Sections.nsh"

; ------------------------------------------------------------------------
; Compile-time defines (with sane fallbacks)
; ------------------------------------------------------------------------

!ifndef PRODUCT_NAME
  !define PRODUCT_NAME "fly-scoreboard"
!endif

!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.0.0"
!endif

!ifndef PROJECT_ROOT
  !define PROJECT_ROOT "."
!endif

!ifndef CONFIGURATION
  !define CONFIGURATION "RelWithDebInfo"
!endif

!ifndef TARGET
  !define TARGET "x64"
!endif

!ifndef OUTPUT_EXE
  !define OUTPUT_EXE "fly-scoreboard-setup.exe"
!endif

; Where CMake installed the plugin:
;   ${PROJECT_ROOT}\release\<CONFIGURATION>\fly-scoreboard\...
!define BUILD_ROOT "${PROJECT_ROOT}\release\${CONFIGURATION}\${PRODUCT_NAME}"

; Installer icon
!ifndef INSTALLER_ICON
  !define INSTALLER_ICON "${PROJECT_ROOT}\installer\resources\fly-scoreboard.ico"
!endif

; Optional custom welcome/finish bitmap
!define MUI_WELCOMEFINISHPAGE_BITMAP "${PROJECT_ROOT}\installer\resources\fly-scoreboard-welcome.bmp"

; ------------------------------------------------------------------------
; Basic installer metadata
; ------------------------------------------------------------------------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${OUTPUT_EXE}"

RequestExecutionLevel admin

Var OBSDir
Var OverlayDir

; ------------------------------------------------------------------------
; Sections (declare BEFORE any functions that use ${SEC_*})
; ------------------------------------------------------------------------

Section "Core OBS Plugin" SEC_CORE
  SectionIn RO

  ; --- Plugin DLL ---
  SetOutPath "$OBSDir\obs-plugins\64bit"
  File "/oname=fly-scoreboard.dll" "${BUILD_ROOT}\bin\64bit\fly-scoreboard.dll"

  ; --- Locale files (future-proof) ---
  SetOutPath "$OBSDir\data\obs-plugins\fly-scoreboard\locale"
  File /nonfatal /r "${BUILD_ROOT}\data\locale\*.*"
SectionEnd

Section "Base Overlay & Web Server Assets" SEC_OVERLAY
  SetOutPath "$OverlayDir"
  File /nonfatal /r "${BUILD_ROOT}\data\overlay\*.*"

  ; If you ever add helper scripts, copy them here as well:
  ; File "${BUILD_ROOT}\data\start-server.bat"
SectionEnd

; ------------------------------------------------------------------------
; MUI pages
; ------------------------------------------------------------------------

!define MUI_ABORTWARNING

; Custom icon for installer + uninstaller
!define MUI_ICON   "${INSTALLER_ICON}"
!define MUI_UNICON "${INSTALLER_ICON}"

!insertmacro MUI_PAGE_WELCOME

; OBS folder selection page
PageEx directory
  DirText "Select the folder where OBS Studio is installed." \
          "The Fly Scoreboard plugin DLL will be installed into this OBS Studio folder." \
          "Browse..."
  DirVar $OBSDir
PageExEnd

; Components page (for overlay checkbox)
!insertmacro MUI_PAGE_COMPONENTS

; Overlay folder selection page – only shown if overlay section is selected
PageEx directory
  PageCallbacks overlayDirPre
  DirText "Select the folder where the base overlay files will be installed." \
          "This is where the HTML/CSS/JS overlay and your local web server will run from." \
          "Browse..."
  DirVar $OverlayDir
PageExEnd

!insertmacro MUI_PAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; ------------------------------------------------------------------------
; Init: default OBS folder
; ------------------------------------------------------------------------

Function .onInit
  ; Default to standard 64-bit OBS install path; user can change it
  StrCpy $OBSDir "$PROGRAMFILES64\obs-studio"
FunctionEnd

; ------------------------------------------------------------------------
; Overlay directory pre-callback
;  - Only show overlay folder page if overlay section is selected
; ------------------------------------------------------------------------

Function overlayDirPre
  SectionGetFlags ${SEC_OVERLAY} $0
  IntOp $0 $0 & ${SF_SELECTED}
  StrCmp $0 0 no_overlay

  ; If no overlay dir chosen yet, default to Documents\fly-scoreboard-overlay
  StrCmp $OverlayDir "" 0 +2
    StrCpy $OverlayDir "$DOCUMENTS\fly-scoreboard-overlay"

  Return

no_overlay:
  ; Skip this page entirely if overlay not selected
  Abort
FunctionEnd

; ------------------------------------------------------------------------
; Version info in EXE properties
; ------------------------------------------------------------------------

VIProductVersion  "${PRODUCT_VERSION}.0"
VIAddVersionKey   "ProductName"     "${PRODUCT_NAME}"
VIAddVersionKey   "FileDescription" "Fly Scoreboard plugin installer"
VIAddVersionKey   "CompanyName"     "MML Tech"
VIAddVersionKey   "FileVersion"     "${PRODUCT_VERSION}"
VIAddVersionKey   "LegalCopyright"  "Copyright © MML Tech"
