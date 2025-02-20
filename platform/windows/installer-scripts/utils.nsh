﻿Var /GLOBAL osArch
Var /GLOBAL sectionDepFlag
Var /GLOBAL sectionDepTemp

!macro DetectOsArch
  ${If} ${IsNativeIA32}
    StrCpy $osArch "x86"
  ${ElseIf} ${IsNativeAMD64}
    StrCpy $osArch "x64"
  ${ElseIf} ${IsNativeARM64}
    StrCpy $osArch "arm64"
  ${Else}
    StrCpy $osArch "unknown"
  ${EndIf}
!macroend

!macro CheckOsArch
  !ifdef STRICT_ARCH_CHECK
    ${If} $osArch != "${ARCH}"
      MessageBox MB_OK|MB_ICONSTOP "$(ErrorStrictArchMismatch)"
      Abort
    ${EndIf}
  !else
    !if "${ARCH}" == "x64"
      ${If} $osArch == "x86"
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorArchMismatch)"
        Abort
      ${EndIf}
    !endif

    !if "${ARCH}" == "arm64"
      ${If} $osArch != "arm64"
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorArchMismatch)"
        Abort
      ${EndIf}
    !endif

    ; warning if not matching
    ${If} $osArch != "${ARCH}"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarningArchMismatch)"
    ${EndIf}
  !endif
!macroend

!macro CheckOsBuild
  ${IfNot} ${IsNT}
  ${OrIfNot} ${AtLeastBuild} ${REQUIRED_WINDOWS_BUILD}
    MessageBox MB_OK|MB_ICONSTOP "$(ErrorWindowsBuildRequired)"
    Abort
  ${EndIf}
!macroend

!macro CheckV2Installer
  SetRegView 32
  Call UninstallV2
  SetRegView 64
  Call UninstallV2
!macroend

!macro CheckPreviousInstaller
  SetRegView 32
  Call UninstallExisting
  SetRegView 64
  Call UninstallExisting
  !if "${ARCH}" == "x86"
    SetRegView 32
  !else
    SetRegView 64
  !endif
!macroend

!macro DisableSection section
  SectionGetFlags ${section} $sectionDepFlag

  ; unset SF_SELECTED
  IntOp $sectionDepTemp ${SF_SELECTED} ~
  IntOp $sectionDepFlag $sectionDepFlag & $sectionDepTemp

  ; set SF_RO
  IntOp $sectionDepFlag $sectionDepFlag | ${SF_RO}

  SectionSetFlags ${section} $sectionDepFlag
!macroend

!macro SectionAction_CheckMingw64
  !ifdef HAVE_MINGW64
    ${If} $osArch == "x86"
      !insertmacro DisableSection ${SectionMingw64}
    ${EndIf}
  !endif
!macroend

!macro SectionAction_CheckCompress
  ; compact os is available since windows 10
  ${IfNot} ${AtLeastBuild} 10240
    !insertmacro DisableSection ${SectionCompress}
  ${EndIf}
!macroend
