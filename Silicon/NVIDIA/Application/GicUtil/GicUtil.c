/** @file
  The main process for GicUtil application.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/ArmGicLib.h>
#include <Library/PcdLib.h>
#include <Protocol/FdtClient.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mGicUtilParamList[] = {
  { L"--test",     TypeFlag  },
  { L"--info",     TypeFlag  },
  { L"--enable",   TypeValue },
  { L"--disable",  TypeValue },
  { L"--priority", TypeValue },
  { L"--sgi",      TypeValue },
  { L"--status",   TypeValue },
  { L"-?",         TypeFlag  },
  { NULL,          TypeMax   },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"GicUtil";

#define MACH_VIRT_GICD_BASE  0x08000000  // MACH_VIRT_PERIPH_BASE for GICD
#define MACH_VIRT_GICR_BASE  0x080A0000  // MACH_VIRT_PERIPH_BASE + 0xA0000 for GICR

// GIC Base addresses from PCD
EFI_PHYSICAL_ADDRESS  GicDistributorBase;
EFI_PHYSICAL_ADDRESS  GicRedistributorBase;
EFI_PHYSICAL_ADDRESS  GicInterruptInterfaceBase;

/**
  Print GIC information
**/
VOID
PrintGicInfo (
  VOID
  )
{
  UINT32                 InterfaceId;
  UINTN                  MaxInterrupts;
  ARM_GIC_ARCH_REVISION  Revision;

  Revision = ArmGicGetSupportedArchRevision ();

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_PROTOCOL_FOUND), mHiiHandle, mAppName);

  // Print GIC architecture revision
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_REVISION), mHiiHandle, mAppName, Revision);

  // Print GIC distributor base
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_DIST_BASE), mHiiHandle, mAppName, GicDistributorBase);

  // Print GIC redistributor base (for GICv3+)
  if (Revision >= ARM_GIC_ARCH_REVISION_3) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_REDIST_BASE), mHiiHandle, mAppName, GicRedistributorBase);
  }

  // Print GIC CPU interface base (for GICv2)
  if (Revision == ARM_GIC_ARCH_REVISION_2) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INTERFACE_BASE), mHiiHandle, mAppName, GicInterruptInterfaceBase);

    // For GICv2, get the interface ID
    InterfaceId = ArmGicGetInterfaceIdentification (GicInterruptInterfaceBase);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_GIC_UTIL_INTERFACE_ID),
      mHiiHandle,
      mAppName,
      ARM_GIC_ICCIIDR_GET_PRODUCT_ID (InterfaceId),
      ARM_GIC_ICCIIDR_GET_ARCH_VERSION (InterfaceId),
      ARM_GIC_ICCIIDR_GET_REVISION (InterfaceId),
      ARM_GIC_ICCIIDR_GET_IMPLEMENTER (InterfaceId)
      );
  }

  // Get and print maximum number of interrupts
  MaxInterrupts = ArmGicGetMaxNumInterrupts (GicDistributorBase);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_MAX_INT), mHiiHandle, mAppName, MaxInterrupts);
}

/**
  Enable a specific interrupt

  @param[in] InterruptId   The interrupt ID to enable
**/
VOID
EnableInterrupt (
  IN UINTN  InterruptId
  )
{
  // Enable the interrupt
  ArmGicEnableInterrupt (GicDistributorBase, GicRedistributorBase, InterruptId);

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_ENABLE_INT), mHiiHandle, mAppName, InterruptId);
}

/**
  Disable a specific interrupt

  @param[in] InterruptId   The interrupt ID to disable
**/
VOID
DisableInterrupt (
  IN UINTN  InterruptId
  )
{
  // Disable the interrupt
  ArmGicDisableInterrupt (GicDistributorBase, GicRedistributorBase, InterruptId);

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_DISABLE_INT), mHiiHandle, mAppName, InterruptId);
}

/**
  Set priority for a specific interrupt

  @param[in] ParamStr   Parameter string in format "InterruptId,Priority"
**/
VOID
SetInterruptPriority (
  IN CONST CHAR16  *ParamStr
  )
{
  CHAR16  *StrIntId;
  CHAR16  *StrPriority;
  CHAR16  *TempStr;
  UINTN   InterruptId;
  UINTN   Priority;

  // Make a copy of the parameter string
  TempStr = AllocateCopyPool (StrSize (ParamStr), ParamStr);
  if (TempStr == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_OUT_OF_RESOURCES), mHiiHandle, mAppName);
    return;
  }

  // Parse the parameter string to get InterruptId and Priority
  StrIntId    = TempStr;
  StrPriority = StrStr (TempStr, L",");

  if (StrPriority == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    FreePool (TempStr);
    return;
  }

  // Replace comma with NULL to split the string
  *StrPriority = L'\0';
  StrPriority++;

  InterruptId = ShellStrToUintn (StrIntId);
  Priority    = ShellStrToUintn (StrPriority);

  if (((InterruptId == 0) && (StrIntId[0] != L'0')) ||
      ((Priority == 0) && (StrPriority[0] != L'0')))
  {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    FreePool (TempStr);
    return;
  }

  // Set the interrupt priority
  ArmGicSetInterruptPriority (GicDistributorBase, GicRedistributorBase, InterruptId, Priority);

  ShellPrintHiiEx (
    -1,
    -1,
    NULL,
    STRING_TOKEN (STR_GIC_UTIL_SET_PRIORITY),
    mHiiHandle,
    mAppName,
    InterruptId,
    Priority
    );

  FreePool (TempStr);
}

/**
  Trigger a Software Generated Interrupt (SGI)

  @param[in] ParamStr   Parameter string in format "SGIID,TargetList,Filter"
                        Where Filter is: 0=TargetList, 1=AllExceptSelf, 2=Self
**/
VOID
SendSgi (
  IN CONST CHAR16  *ParamStr
  )
{
  CHAR16  *StrSgiId;
  CHAR16  *StrTargetList;
  CHAR16  *StrFilter;
  CHAR16  *TempStr;
  CHAR16  *TempPtr;
  UINT8   SgiId;
  UINT8   TargetList;
  UINT8   Filter;
  UINTN   ReceivedInterruptId;
  UINTN   RegisterValue;

  // Make a copy of the parameter string
  TempStr = AllocateCopyPool (StrSize (ParamStr), ParamStr);
  if (TempStr == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_OUT_OF_RESOURCES), mHiiHandle, mAppName);
    return;
  }

  // Parse the parameter string
  StrSgiId      = TempStr;
  StrTargetList = StrStr (TempStr, L",");

  if (StrTargetList == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    FreePool (TempStr);
    return;
  }

  // Replace first comma with NULL to split the string
  *StrTargetList = L'\0';
  StrTargetList++;

  // Find the second comma for the filter
  StrFilter = StrStr (StrTargetList, L",");

  if (StrFilter == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    FreePool (TempStr);
    return;
  }

  // Replace second comma with NULL
  *StrFilter = L'\0';
  StrFilter++;

  // Convert string parameters to integers
  SgiId      = (UINT8)ShellStrToUintn (StrSgiId);
  TargetList = (UINT8)ShellStrToUintn (StrTargetList);
  Filter     = (UINT8)ShellStrToUintn (StrFilter);

  // Validate parameters, SGI ID must be between 0 and 15, Filter must be between 0 and 2, and the parameters must not have conversion errors
  if ((SgiId > 15) || (Filter > 2) ||
      ((SgiId == 0) && (StrSgiId[0] != L'0')) ||
      ((TargetList == 0) && (StrTargetList[0] != L'0')) ||
      ((Filter == 0) && (StrFilter[0] != L'0')))
  {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    FreePool (TempStr);
    return;
  }

  // Send the SGI
  ArmGicSendSgiTo (GicDistributorBase, Filter, TargetList, SgiId);

  // Print the operation that occurred
  if (Filter == ARM_GIC_ICDSGIR_FILTER_TARGETLIST) {
    TempPtr = L"TargetList";
  } else if (Filter == ARM_GIC_ICDSGIR_FILTER_EVERYONEELSE) {
    TempPtr = L"EveryoneElse";
  } else {
    TempPtr = L"Self";
  }

  ShellPrintHiiEx (
    -1,
    -1,
    NULL,
    STRING_TOKEN (STR_GIC_UTIL_SEND_SGI),
    mHiiHandle,
    mAppName,
    SgiId,
    TargetList,
    TempPtr
    );

  // Check if the SGI was acknowledged
  if (Filter == ARM_GIC_ICDSGIR_FILTER_ITSELF) {
    // If we're sending to self, we can check acknowledgement
    RegisterValue = ArmGicAcknowledgeInterrupt (GicInterruptInterfaceBase, &ReceivedInterruptId);

    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_GIC_UTIL_INT_ACK),
      mHiiHandle,
      mAppName,
      ReceivedInterruptId,
      RegisterValue
      );

    // End the interrupt (required after acknowledging)
    ArmGicEndOfInterrupt (GicInterruptInterfaceBase, RegisterValue);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_NO_ACK), mHiiHandle, mAppName);
  }

  FreePool (TempStr);
}

/**
  Check if an interrupt is enabled

  @param[in] InterruptId   The interrupt ID to check
**/
VOID
CheckInterruptStatus (
  IN UINTN  InterruptId
  )
{
  BOOLEAN  IsEnabled;

  // Check if the interrupt is enabled
  IsEnabled = ArmGicIsInterruptEnabled (GicDistributorBase, GicRedistributorBase, InterruptId);

  if (IsEnabled) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INT_ENABLED), mHiiHandle, mAppName, InterruptId);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INT_DISABLED), mHiiHandle, mAppName, InterruptId);
  }
}

/**
  Detect GIC addresses from Device Tree if available
  Based on edk2/ArmVirtPkg/Library/ArmVirtGicArchLib/ArmVirtGicArchLib.c
**/
EFI_STATUS
DetectGicAddressesFromDeviceTree (
  VOID
  )
{
  FDT_CLIENT_PROTOCOL  *FdtClient;
  CONST UINT64         *Reg;
  UINT32               RegSize;
  UINTN                AddressCells, SizeCells;
  UINTN                GicRevision;
  EFI_STATUS           Status;

  // Try to locate the FDT Client Protocol
  Status = gBS->LocateProtocol (
                  &gFdtClientProtocolGuid,
                  NULL,
                  (VOID **)&FdtClient
                  );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_FDT_NOT_FOUND), mHiiHandle, mAppName, Status);
    return Status;
  }

  // First try to find GICv3
  GicRevision = 3;
  Status      = FdtClient->FindCompatibleNodeReg (
                             FdtClient,
                             "arm,gic-v3",
                             (CONST VOID **)&Reg,
                             &AddressCells,
                             &SizeCells,
                             &RegSize
                             );

  // If not found, try GICv2
  if (Status == EFI_NOT_FOUND) {
    GicRevision = 2;
    Status      = FdtClient->FindCompatibleNodeReg (
                               FdtClient,
                               "arm,cortex-a15-gic",
                               (CONST VOID **)&Reg,
                               &AddressCells,
                               &SizeCells,
                               &RegSize
                               );
  }

  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_NO_GIC_NODE), mHiiHandle, mAppName, Status);
    return Status;
  }

  switch (GicRevision) {
    case 3:
      // For GICv3, first pair is distributor, second is redistributor
      if (RegSize < 32) {
        return EFI_INVALID_PARAMETER;
      }

      GicDistributorBase   = SwapBytes64 (Reg[0]);
      GicRedistributorBase = SwapBytes64 (Reg[2]);

      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_FOUND_GICV3), mHiiHandle, mAppName, GicDistributorBase, GicRedistributorBase);
      break;

    case 2:
      // For GICv2, first pair is distributor, second is CPU interface
      if ((RegSize != 32) && (RegSize != 64)) {
        return EFI_INVALID_PARAMETER;
      }

      GicDistributorBase        = SwapBytes64 (Reg[0]);
      GicInterruptInterfaceBase = SwapBytes64 (Reg[2]);

      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_FOUND_GICV2), mHiiHandle, mAppName, GicDistributorBase, GicInterruptInterfaceBase);
      break;

    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Initialize GIC base addresses from the dynamic PCD table
**/
EFI_STATUS
InitializeGicBaseAddresses (
  VOID
  )
{
  EFI_STATUS  Status;

  // First try to get addresses from Device Tree directly
  Status = DetectGicAddressesFromDeviceTree ();
  if (!EFI_ERROR (Status)) {
    // Successfully got addresses from Device Tree
    return EFI_SUCCESS;
  }

  // If Device Tree detection failed, fall back to values from PcdGet
  GicDistributorBase        = PcdGet64 (PcdGicDistributorBase);
  GicRedistributorBase      = PcdGet64 (PcdGicRedistributorsBase);
  GicInterruptInterfaceBase = PcdGet64 (PcdGicInterruptInterfaceBase);

  // If addresses are not valid or not aligned to 4KB, use hardcoded values that are known to work with edk2 virt
  if ((GicDistributorBase == 0) || !IS_ALIGNED (GicDistributorBase, SIZE_4KB)) {
    GicDistributorBase = MACH_VIRT_GICD_BASE;
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_HARDCODED_GICD), mHiiHandle, mAppName, GicDistributorBase);
  }

  if ((GicRedistributorBase == 0) || !IS_ALIGNED (GicRedistributorBase, SIZE_4KB)) {
    GicRedistributorBase = MACH_VIRT_GICR_BASE;
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_HARDCODED_GICR), mHiiHandle, mAppName, GicRedistributorBase);
  }

  return EFI_SUCCESS;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for GicUtil application that parse the command line input.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
InitializeGicUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  LIST_ENTRY                   *ParamPackage;
  CONST CHAR16                 *ValueStr;
  CHAR16                       *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  //
  // Retrieve HII package list from ImageHandle
  //
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

  Status = ShellCommandLineParseEx (mGicUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    goto Done;
  }

  // Get GIC base addresses from PCD
  Status = InitializeGicBaseAddresses ();
  if (EFI_ERROR (Status) || (GicDistributorBase == 0)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_NO_GIC), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--test")) {
    BOOLEAN  TestPassed = TRUE;
    BOOLEAN  IsEnabled;

    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_START), mHiiHandle, mAppName);
    PrintGicInfo ();

    // Test case 1: Enable interrupt 9
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 1, L"Enable interrupt 9");
    EnableInterrupt (9);

    // Verify interrupt 9 is enabled
    IsEnabled = ArmGicIsInterruptEnabled (GicDistributorBase, GicRedistributorBase, 9);
    if (!IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 1);
      TestPassed = FALSE;
      goto TestDone;
    }

    // Test case 2: Set priority for interrupt 9 to 1
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 2, L"Set priority for interrupt 9");
    SetInterruptPriority (L"9,1");

    // Test case 3: Check if interrupt 9 is enabled
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 4, L"Verify interrupt 9 is enabled");
    CheckInterruptStatus (9);
    IsEnabled = ArmGicIsInterruptEnabled (GicDistributorBase, GicRedistributorBase, 9);
    if (!IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 4);
      TestPassed = FALSE;
      goto TestDone;
    }

    // Test case 4: Disable interrupt 9
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 5, L"Disable interrupt 9");
    DisableInterrupt (9);

    // Test case 5: Check if interrupt 9 is disabled
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 6, L"Verify interrupt 9 is disabled");
    CheckInterruptStatus (9);
    IsEnabled = ArmGicIsInterruptEnabled (GicDistributorBase, GicRedistributorBase, 9);
    if (IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 6);
      TestPassed = FALSE;
      goto TestDone;
    }

TestDone:
    // Only print success if all tests passed
    if (TestPassed) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_PASSED_ALL), mHiiHandle, mAppName);
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_SUMMARY_FAILED), mHiiHandle, mAppName);
    }

    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--info")) {
    PrintGicInfo ();
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--enable");
  if (ValueStr != NULL) {
    UINTN  InterruptId = ShellStrToUintn (ValueStr);
    EnableInterrupt (InterruptId);
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--disable");
  if (ValueStr != NULL) {
    UINTN  InterruptId = ShellStrToUintn (ValueStr);
    DisableInterrupt (InterruptId);
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--priority");
  if (ValueStr != NULL) {
    SetInterruptPriority (ValueStr);
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--sgi");
  if (ValueStr != NULL) {
    SendSgi (ValueStr);
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--status");
  if (ValueStr != NULL) {
    UINTN  InterruptId = ShellStrToUintn (ValueStr);
    CheckInterruptStatus (InterruptId);
    goto Done;
  }

  // If here, no valid command was specified
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_HELP), mHiiHandle);

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
