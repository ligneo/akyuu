; Copyright (C) 2010-2024, Eren Okka
; Copyright (C) 2026, cenky <cenkkgl@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later
!include "MUI2.nsh"
!include "x64.nsh"
!include "StrFunc.nsh"
${StrStr}
${UnStrStr}
!ifndef STAGING_DIR
  !error "Set STAGING_DIR to the deployed application directory"
!endif
!ifndef OUTPUT_FILE
  !error "Set OUTPUT_FILE"
!endif
!ifndef PRODUCT_VERSION
  !error "Set PRODUCT_VERSION to the numeric application version"
!endif
!ifndef DISPLAY_VERSION
  !error "Set DISPLAY_VERSION to the release version"
!endif
!ifndef UNINSTALL_MANIFEST
  !error "Set UNINSTALL_MANIFEST to the generated owned-file removal script"
!endif
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Akyuu"

Name "Akyuu"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\Akyuu"
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "Akyuu"
VIAddVersionKey "FileDescription" "Akyuu Setup"
VIAddVersionKey "FileVersion" "${DISPLAY_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright (C) Eren Okka and cenky"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\Akyuu.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Akyuu requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
  nsExec::ExecToStack '"$SYSDIR\tasklist.exe" /FI "IMAGENAME eq Akyuu.exe" /NH'
  Pop $0
  Pop $1
  ${StrStr} $2 $1 "Akyuu.exe"
  ${If} $0 != 0
  ${OrIf} $2 != ""
    MessageBox MB_ICONSTOP "Close Akyuu before installing or updating." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
FunctionEnd

Section "Akyuu"
  SetOutPath "$INSTDIR"
  File /r "${STAGING_DIR}\*"
  ExecWait '"$INSTDIR\vc_redist.x64.exe" /install /quiet /norestart' $0
  ${If} $0 != 0
  ${AndIf} $0 != 3010
  ${AndIf} $0 != 1638
    MessageBox MB_ICONSTOP "The Microsoft runtime could not be installed ($0)." /SD IDOK
    SetErrorLevel 3
    Abort
  ${EndIf}
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "Akyuu"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${DISPLAY_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "ligneo"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\Akyuu"
  CreateShortCut "$SMPROGRAMS\Akyuu\Akyuu.lnk" "$INSTDIR\Akyuu.exe"
  CreateShortCut "$SMPROGRAMS\Akyuu\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section Uninstall
  SetRegView 64
  SetShellVarContext all
  nsExec::ExecToStack '"$SYSDIR\tasklist.exe" /FI "IMAGENAME eq Akyuu.exe" /NH'
  Pop $0
  Pop $1
  ${UnStrStr} $2 $1 "Akyuu.exe"
  ${If} $0 != 0
  ${OrIf} $2 != ""
    MessageBox MB_ICONSTOP "Close Akyuu before uninstalling." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  DeleteRegKey HKLM "${UNINST_KEY}"
  Delete "$SMPROGRAMS\Akyuu\Akyuu.lnk"
  Delete "$SMPROGRAMS\Akyuu\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Akyuu"
  ; Keep the current working directory outside the tree being removed.
  SetOutPath "$TEMP"
  ; Remove only packaged files. Empty-directory removal preserves unrelated files.
  !include "${UNINSTALL_MANIFEST}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
