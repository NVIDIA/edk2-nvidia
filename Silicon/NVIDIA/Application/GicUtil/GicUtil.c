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
#include <Library/PcdLib.h>
#include <Library/ArmLib.h>
#include <Protocol/FdtClient.h>
#include <Library/IoLib.h>

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
  { L"--status",   TypeValue },
  { L"-?",         TypeFlag  },
  { NULL,          TypeMax   },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"GicUtil";

// GIC Distributor
#define ARM_GIC_ICDISER  0x100        // Interrupt Set-Enable Registers
#define ARM_GIC_ICDICER  0x180        // Interrupt Clear-Enable Registers
#define ARM_GIC_ICDIPR   0x400        // Interrupt Priority Registers

// GIC Redistributor
#define ARM_GICR_CTLR_FRAME_SIZE          SIZE_64KB
#define ARM_GICR_SGI_PPI_FRAME_SIZE       SIZE_64KB
#define ARM_GICR_SGI_VLPI_FRAME_SIZE      SIZE_64KB
#define ARM_GICR_SGI_RESERVED_FRAME_SIZE  SIZE_64KB

// GIC Redistributor Control frame
#define ARM_GICR_TYPER  0x0008          // Redistributor Type Register

// GIC SGI & PPI Redistributor frame
#define ARM_GICR_ISENABLER  0x0100      // Interrupt Set-Enable Registers
#define ARM_GICR_ICENABLER  0x0180      // Interrupt Clear-Enable Registers

#define ARM_GICR_TYPER_LAST      (1 << 4)                 // Last Redistributor in series
#define ARM_GICR_TYPER_AFFINITY  (0xFFFFFFFFULL << 32)    // Redistributor Affinity

#define ARM_GICR_TYPER_GET_AFFINITY(TypeReg)  (((TypeReg) & \
                                                ARM_GICR_TYPER_AFFINITY) >> 32)

#define MACH_VIRT_GICD_BASE  0x08000000  // MACH_VIRT_PERIPH_BASE for GICD
#define MACH_VIRT_GICR_BASE  0x080A0000  // MACH_VIRT_PERIPH_BASE + 0xA0000 for GICR

// GIC Base addresses from PCD
EFI_PHYSICAL_ADDRESS  GicDistributorBase;
EFI_PHYSICAL_ADDRESS  GicRedistributorBase;

// Helper macros for GIC register access
#define ISENABLER_ADDRESS(base, offset)  ((base) +\
          ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_ISENABLER + 4 * (offset))

#define ICENABLER_ADDRESS(base, offset)  ((base) +\
          ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_ICENABLER + 4 * (offset))

#define IPRIORITY_ADDRESS(base, offset)  ((base) +\
          ARM_GICR_CTLR_FRAME_SIZE + ARM_GIC_ICDIPR + 4 * (offset))

/**
  Print GIC information
**/
VOID
PrintGicInfo (
  VOID
  )
{
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_PROTOCOL_FOUND), mHiiHandle, mAppName);

  // Print GIC architecture revision
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_REVISION), mHiiHandle, mAppName, 3);

  // Print GIC distributor base
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_DIST_BASE), mHiiHandle, mAppName, GicDistributorBase);

  // Print GIC redistributor base
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_REDIST_BASE), mHiiHandle, mAppName, GicRedistributorBase);
}

/**
  Return whether the Source interrupt index refers to a shared interrupt (SPI)
**/
STATIC
BOOLEAN
SourceIsSpi (
  IN UINTN  Source
  )
{
  return Source >= 32 && Source < 1020;
}

/**
  Return the base address of the GIC redistributor for the current CPU
**/
STATIC
UINTN
GicGetCpuRedistributorBase (
  IN UINTN  GicRedistributorBase
  )
{
  UINTN   MpId;
  UINTN   CpuAffinity;
  UINTN   Affinity;
  UINTN   GicCpuRedistributorBase;
  UINT64  TypeRegister;

  MpId = ArmReadMpidr ();
  // Define CPU affinity as:
  // Affinity0[0:8], Affinity1[9:15], Affinity2[16:23], Affinity3[24:32]
  // whereas Affinity3 is defined at [32:39] in MPIDR
  CpuAffinity = (MpId & (ARM_CORE_AFF0 | ARM_CORE_AFF1 | ARM_CORE_AFF2)) |
                ((MpId & ARM_CORE_AFF3) >> 8);

  GicCpuRedistributorBase = GicRedistributorBase;

  do {
    TypeRegister = MmioRead64 (GicCpuRedistributorBase + ARM_GICR_TYPER);
    Affinity     = ARM_GICR_TYPER_GET_AFFINITY (TypeRegister);
    if (Affinity == CpuAffinity) {
      return GicCpuRedistributorBase;
    }

    // Move to the next GIC Redistributor frame
    GicCpuRedistributorBase += ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_SGI_PPI_FRAME_SIZE;
  } while ((TypeRegister & ARM_GICR_TYPER_LAST) == 0);

  return 0;
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
  UINT32  RegOffset;
  UINT8   RegShift;
  UINTN   GicCpuRedistributorBase;

  // Calculate enable register offset and bit position
  RegOffset = (UINT32)(InterruptId / 32);
  RegShift  = (UINT8)(InterruptId % 32);

  if (SourceIsSpi (InterruptId)) {
    // Write set-enable register
    MmioWrite32 (
      GicDistributorBase + ARM_GIC_ICDISER + (4 * RegOffset),
      1 << RegShift
      );
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase (GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_ERROR), mHiiHandle, mAppName, EFI_NOT_FOUND);
      return;
    }

    // Write set-enable register
    MmioWrite32 (
      ISENABLER_ADDRESS (GicCpuRedistributorBase, RegOffset),
      1 << RegShift
      );
  }

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
  UINT32  RegOffset;
  UINT8   RegShift;
  UINTN   GicCpuRedistributorBase;

  // Calculate enable register offset and bit position
  RegOffset = (UINT32)(InterruptId / 32);
  RegShift  = (UINT8)(InterruptId % 32);

  if (SourceIsSpi (InterruptId)) {
    // Write clear-enable register
    MmioWrite32 (
      GicDistributorBase + ARM_GIC_ICDICER + (4 * RegOffset),
      1 << RegShift
      );
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase (GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_ERROR), mHiiHandle, mAppName, EFI_NOT_FOUND);
      return;
    }

    // Write clear-enable register
    MmioWrite32 (
      ICENABLER_ADDRESS (GicCpuRedistributorBase, RegOffset),
      1 << RegShift
      );
  }

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
  UINT32  RegOffset;
  UINT8   RegShift;
  UINTN   GicCpuRedistributorBase;

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

  // Calculate register offset and bit position
  RegOffset = (UINT32)(InterruptId / 4);
  RegShift  = (UINT8)((InterruptId % 4) * 8);

  if (SourceIsSpi (InterruptId)) {
    MmioAndThenOr32 (
      GicDistributorBase + ARM_GIC_ICDIPR + (4 * RegOffset),
      ~(0xff << RegShift),
      Priority << RegShift
      );
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase (GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_ERROR), mHiiHandle, mAppName, EFI_NOT_FOUND);
      FreePool (TempStr);
      return;
    }

    MmioAndThenOr32 (
      IPRIORITY_ADDRESS (GicCpuRedistributorBase, RegOffset),
      ~(0xff << RegShift),
      Priority << RegShift
      );
  }

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
  Check if an interrupt is enabled

  @param[in] InterruptId   The interrupt ID to check
  @return TRUE if the interrupt is enabled, FALSE otherwise
**/
BOOLEAN
CheckInterruptStatus (
  IN UINTN  InterruptId
  )
{
  UINT32   RegOffset;
  UINT8    RegShift;
  UINTN    GicCpuRedistributorBase;
  UINT32   Interrupts;
  BOOLEAN  IsEnabled;

  // Calculate enable register offset and bit position
  RegOffset = (UINT32)(InterruptId / 32);
  RegShift  = (UINT8)(InterruptId % 32);

  if (SourceIsSpi (InterruptId)) {
    Interrupts = MmioRead32 (
                   GicDistributorBase + ARM_GIC_ICDISER + (4 * RegOffset)
                   );
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase (GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_ERROR), mHiiHandle, mAppName, EFI_NOT_FOUND);
      return FALSE;
    }

    // Read set-enable register
    Interrupts = MmioRead32 (
                   ISENABLER_ADDRESS (GicCpuRedistributorBase, RegOffset)
                   );
  }

  IsEnabled = ((Interrupts & (1 << RegShift)) != 0);

  if (IsEnabled) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INT_ENABLED), mHiiHandle, mAppName, InterruptId);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_INT_DISABLED), mHiiHandle, mAppName, InterruptId);
  }

  return IsEnabled;
}

/**
  Detect GIC addresses from Device Tree if available
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

  // Find GICv3 node
  Status = FdtClient->FindCompatibleNodeReg (
                        FdtClient,
                        "arm,gic-v3",
                        (CONST VOID **)&Reg,
                        &AddressCells,
                        &SizeCells,
                        &RegSize
                        );

  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_NO_GIC_NODE), mHiiHandle, mAppName, Status);
    return Status;
  }

  // For GICv3, first pair is distributor, second is redistributor
  if (RegSize < 32) {
    return EFI_INVALID_PARAMETER;
  }

  GicDistributorBase   = SwapBytes64 (Reg[0]);
  GicRedistributorBase = SwapBytes64 (Reg[2]);

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_FOUND_GICV3), mHiiHandle, mAppName, GicDistributorBase, GicRedistributorBase);

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
  GicDistributorBase   = PcdGet64 (PcdGicDistributorBase);
  GicRedistributorBase = PcdGet64 (PcdGicRedistributorsBase);

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
    IsEnabled = CheckInterruptStatus (9);
    if (!IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 1);
      TestPassed = FALSE;
      goto TestDone;
    }

    // Test case 2: Set priority for interrupt 9 to 1
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 2, L"Set priority for interrupt 9");
    SetInterruptPriority (L"9,1");

    // Test case 3: Check if interrupt 9 is enabled
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 3, L"Verify interrupt 9 is enabled");
    IsEnabled = CheckInterruptStatus (9);
    if (!IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 3);
      TestPassed = FALSE;
      goto TestDone;
    }

    // Test case 4: Disable interrupt 9
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 4, L"Disable interrupt 9");
    DisableInterrupt (9);

    // Test case 5: Check if interrupt 9 is disabled
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_CASE), mHiiHandle, mAppName, 5, L"Verify interrupt 9 is disabled");
    IsEnabled = CheckInterruptStatus (9);
    if (IsEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GIC_UTIL_TEST_FAILED), mHiiHandle, mAppName, 5);
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
