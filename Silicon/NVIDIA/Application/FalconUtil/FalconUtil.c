/** @file
  The main process for FalconUtil application.

  Copyright (c) 2019-2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HiiLib.h>
#include <Library/UsbFalconLib.h>
#include <Protocol/XhciController.h>

#define AUTO_INCREMENT_ON_READ_DM  0x2000000
#define AUTO_INCREMENT_ON_READ_DD  0x80000000
#define DDIRECT_OFFSET             0x2000
#define FALCON_DMEMC               0x1C0
#define FALCON_DMEMD               0x1C4
#define MEMPOOL_REGACCESS_MEMC     0x101A50
#define MEMPOOL_REGACCESS_MEMD     0x101A54
#define MEMPOOL_REGACCESS_DEST     0x101A58
#define DEST_TGT_DDIRECT           0x0

#define TRACEIDX                0x148
#define TRACEIDX_MAXINDX_MASK   0xff
#define TRACEIDX_MAXINDX_SHIFT  16
#define TRACEPC                 0x14c
#define ICD_CMD                 0x200
#define ICD_CMD_OPC_RSTAT       0xe
#define ICD_CMD_OPC_RREG        0x8
#define ICD_CMD_IDX_SHIFT       0x8
#define TOTAL_REG_COUNT         29
#define TOTAL_RSTAT_COUNT       6
#define ICD_RDATA               0x20c
#define FALCON_CPUCTL           0x100

/* Used for ShellCommandLineParseEx only
 * and to ensure user inputs are in valid format
 */
SHELL_PARAM_ITEM  mFalconUtilParamList[] = {
  { L"-r",    TypeValue },
  { L"-w",    TypeValue },
  { L"-dd",   TypeValue },
  { L"-dm",   TypeValue },
  { L"-diag", TypeFlag  },
  { L"-?",    TypeFlag  },
  { NULL,     TypeMax   },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[]                  = L"FalconUtil";
CONST CHAR16    mIcdReg[TOTAL_REG_COUNT][5] = {
  L"R00", L"R01", L"R02", L"R03", L"R04", L"R05", L"R06", L"R07", L"R08", L"R09", L"R10", L"R11", L"R12",
  L"R13", L"R14", L"R15", L"IV0", L"IV1", L"\0",  L"EV",  L"SP",  L"PC",  L"IMB", L"DMB", L"CSW", L"CCR",L"SEC",L"CTX", L"EXCI"
};

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for FalconUtil application that parse the command line input
  and call a Falcon command.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
InitializeFalconUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_HII_PACKAGE_LIST_HEADER     *PackageList;
  LIST_ENTRY                      *ParamPackage;
  CHAR16                          *ProblemParam;
  UINTN                           Position;
  CONST CHAR16                    *ValueStr;
  UINTN                           Address;
  UINTN                           Value;
  UINT32                          Value32;
  UINT32                          NumDwords;
  UINT32                          Iter;
  UINT32                          PrintAddress;
  EFI_PHYSICAL_ADDRESS            CfgAddress = 0;
  NVIDIA_XHCICONTROLLER_PROTOCOL  *mXhciControllerProtocol;
  UINT32                          MaxIndex;

  // Retrieve HII package list from ImageHandle
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &mHiiHandle
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (
             mFalconUtilParamList,
             &ParamPackage,
             &ProblemParam,
             TRUE,
             FALSE
             );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_UNKNOWN_OPERATION),
      mHiiHandle,
      ProblemParam
      );
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_HELP),
      mHiiHandle,
      mAppName
      );
    goto Done;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIAXhciControllerProtocolGuid,
                  NULL,
                  (VOID **)&mXhciControllerProtocol
                  );
  if (EFI_ERROR (Status) || (mXhciControllerProtocol == NULL)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: Can't get XhciController Protocol Handle:%r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  /* Get the XHCI Config Registers Base Address */
  mXhciControllerProtocol->GetCfgAddr (mXhciControllerProtocol, &CfgAddress);
  if (CfgAddress == 0) {
    DEBUG ((EFI_D_ERROR, "%a: Invalid Xhci Config Address Received\n"));
    goto Done;
  }

  /* Set the XHCI Config Address in Falcon Library before using any other
   * functions of the Library
   */
  FalconSetHostCfgAddr (CfgAddress);

  /* Print Diagnostic Info used for debugging Firmware Halts */
  if (ShellCommandLineGetFlag (ParamPackage, L"-diag")) {
    /* Print Falcon CPU Status */
    Value32 = FalconRead32 (FALCON_CPUCTL);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_REG_VALUE),
      mHiiHandle,
      L"FALCON_CPUCTL[0x100]",
      Value32
      );
    /* Display In Circuit Debug Registers Information */
    for (Iter = 0; Iter < TOTAL_REG_COUNT; Iter++) {
      if (StrCmp (mIcdReg[Iter], L"\0") == 0) {
        continue;
      }

      Value32 = (Iter << ICD_CMD_IDX_SHIFT) | ICD_CMD_OPC_RREG;
      FalconWrite32 (ICD_CMD, Value32);
      Value32 = FalconRead32 (ICD_RDATA);
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_REG_VALUE),
        mHiiHandle,
        mIcdReg[Iter],
        Value32
        );
    }

    for (Iter = 0; Iter < TOTAL_RSTAT_COUNT; Iter++) {
      Value32 = (Iter << ICD_CMD_IDX_SHIFT) | ICD_CMD_OPC_RSTAT;
      FalconWrite32 (ICD_CMD, Value32);
      Value32 = FalconRead32 (ICD_RDATA);
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_RSTAT),
        mHiiHandle,
        Iter,
        Value32
        );
    }

    /* Displaying Program Counter Trace */
    Value32  = FalconRead32 (TRACEIDX);
    MaxIndex = (Value32 >> TRACEIDX_MAXINDX_SHIFT) & TRACEIDX_MAXINDX_MASK;
    for (Iter = 0; Iter <= MaxIndex; Iter++) {
      FalconWrite32 (TRACEIDX, Iter);
      Value32 = FalconRead32 (TRACEPC);
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_TRACE_PC),
        mHiiHandle,
        Iter,
        Value32
        );
    }
  }

  if ((ValueStr = ShellCommandLineGetValue (ParamPackage, L"-r")) != NULL) {
    /* address */
    Value   = ShellStrToUintn (ValueStr);
    Address = Value;
    /* read register */
    Value32 = FalconRead32 (Address);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_READ_INFO),
      mHiiHandle,
      Address,
      Value32
      );
    goto Done;
  }

  if ((ValueStr = ShellCommandLineGetValue (ParamPackage, L"-w")) != NULL) {
    /* address */
    Value   = ShellStrToUintn (ValueStr);
    Address = Value;
    /* data */
    ValueStr = ShellCommandLineGetRawValue (ParamPackage, 1);
    if (ValueStr == NULL) {
      DEBUG ((EFI_D_ERROR, "\nwrite value not provided\n\n", Value));
      goto Done;
    }

    Value   = ShellStrToUintn (ValueStr);
    Value32 = (UINT32)Value;
    /* write register */
    Value32 = FalconWrite32 (Address, Value32);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_WRITE_INFO),
      mHiiHandle,
      Address,
      Value32
      );
    goto Done;
  }

  if ((ValueStr = ShellCommandLineGetValue (ParamPackage, L"-dm")) != NULL) {
    /* Offset into DMEM */
    Value = ShellStrToUintn (ValueStr);
    if (Value >= DDIRECT_OFFSET) {
      DEBUG ((
        EFI_D_ERROR,
        "\nDMEM Offset should be less than DMEM Size(0x2000)\n\n"
        ));
      goto Done;
    }

    PrintAddress = Value;
    Address      = Value | AUTO_INCREMENT_ON_READ_DM;
    /* Num of DWORDS to read */
    ValueStr = ShellCommandLineGetRawValue (ParamPackage, 1);
    if (ValueStr == NULL) {
      DEBUG ((
        EFI_D_ERROR,
        "\nProvide number of DWORDS to read from DMEM\n\n"
        ));
      goto Done;
    }

    Value     = ShellStrToUintn (ValueStr);
    NumDwords = (UINT32)Value;
    /* write Dmem Offset to start Reading From */
    Value32 = FalconWrite32 (FALCON_DMEMC, Address);

    for (Iter = 0; Iter < NumDwords; Iter++) {
      if ((Iter % 4) == 0) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_NEW_LINE_OFFSET),
          mHiiHandle,
          PrintAddress
          );
        PrintAddress += 0x10;
      }

      Value32 = FalconRead32 (FALCON_DMEMD);
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_DATA),
        mHiiHandle,
        Value32
        );
    }

    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_NEW_LINE),
      mHiiHandle,
      PrintAddress
      );
    goto Done;
  }

  if ((ValueStr = ShellCommandLineGetValue (ParamPackage, L"-dd")) != NULL) {
    /* Offset into DMEM */
    Value = ShellStrToUintn (ValueStr);
    if (Value < DDIRECT_OFFSET) {
      DEBUG ((
        EFI_D_ERROR,
        "\nAddress should be more than DDIRECT Start Address(0x2000)\n\n"
        ));
      goto Done;
    }

    PrintAddress = Value;
    Address      = (Value - DDIRECT_OFFSET) | AUTO_INCREMENT_ON_READ_DD;
    /* Num of DWORDS to read */
    ValueStr = ShellCommandLineGetRawValue (ParamPackage, 1);
    if (ValueStr == NULL) {
      DEBUG ((
        EFI_D_ERROR,
        "\nProvide number of DWORDS to Read from DDIRECT\n\n"
        ));
      goto Done;
    }

    Value     = ShellStrToUintn (ValueStr);
    NumDwords = (UINT32)Value;
    /* write DDIRECT Offset to start Reading From Mempool */
    Value32 = FalconWrite32 (MEMPOOL_REGACCESS_MEMC, Address);
    /* Program Destination to be DDIRECT */
    Value32 = FalconWrite32 (MEMPOOL_REGACCESS_DEST, DEST_TGT_DDIRECT);

    for (Iter = 0; Iter < NumDwords; Iter++) {
      if ((Iter % 4) == 0) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_NEW_LINE_OFFSET),
          mHiiHandle,
          PrintAddress
          );
        PrintAddress += 0x10;
      }

      Value32 = FalconRead32 (MEMPOOL_REGACCESS_MEMD);
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_DATA),
        mHiiHandle,
        Value32
        );
    }

    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_NEW_LINE),
      mHiiHandle
      );
    goto Done;
  }

  for (Position = 1; (ValueStr = ShellCommandLineGetRawValue (ParamPackage, Position)) != NULL; Position++) {
    /* address */
    if (Position == 1) {
      Value   = ShellStrToUintn (ValueStr);
      Address = Value;
    }

    /* data */
    if (Position == 2) {
      Value   = ShellStrToUintn (ValueStr);
      Value32 = (UINT32)Value;
    }
  }

  /* read register */
  if (Position == 2) {
    Value32 = FalconRead32 (Address);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_READ_INFO),
      mHiiHandle,
      Address,
      Value32
      );
  }

  /* write register */
  if (Position == 3) {
    Value32 = FalconWrite32 (Address, Value32);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_FALCON_UTIL_DISPLAY_WRITE_INFO),
      mHiiHandle,
      Address,
      Value32
      );
  }

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return Status;
}
