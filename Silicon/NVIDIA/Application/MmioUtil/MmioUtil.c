/** @file
  MMIO Utility - Protected MMIO Read/Write for UEFI Shell.

  This application safely performs MMIO reads/writes by mapping unmapped
  addresses into the GCD memory map before access, preventing page faults
  on addresses not in the UEFI memory map.

  Additionally, it installs an exception handler to catch Data Aborts
  (translation faults, permission faults, bus errors) and report them
  gracefully instead of crashing.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Pi/PiDxeCis.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/HiiLib.h>
#include <Library/IoLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Protocol/Cpu.h>

//
// AArch64 Exception types
//
#define EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS  0

//
// ESR_ELx Exception Class (EC) field values for AArch64
//
#define ESR_EC_SHIFT           26
#define ESR_EC_MASK            0x3F
#define ESR_EC_DATA_ABORT_EL0  0x24   // Data Abort from lower EL
#define ESR_EC_DATA_ABORT      0x25   // Data Abort from current EL (EL1/EL2)
#define ESR_EC_SERROR          0x2F   // SError interrupt

//
// DFSC (Data Fault Status Code) values
//
#define ESR_DFSC_MASK  0x3F
// Translation faults
#define ESR_DFSC_TRANS_FAULT_L0  0x04
#define ESR_DFSC_TRANS_FAULT_L1  0x05
#define ESR_DFSC_TRANS_FAULT_L2  0x06
#define ESR_DFSC_TRANS_FAULT_L3  0x07
// Access flag faults
#define ESR_DFSC_ACCESS_FLAG_L0  0x08
#define ESR_DFSC_ACCESS_FLAG_L1  0x09
#define ESR_DFSC_ACCESS_FLAG_L2  0x0A
#define ESR_DFSC_ACCESS_FLAG_L3  0x0B
// Permission faults
#define ESR_DFSC_PERM_FAULT_L0  0x0C
#define ESR_DFSC_PERM_FAULT_L1  0x0D
#define ESR_DFSC_PERM_FAULT_L2  0x0E
#define ESR_DFSC_PERM_FAULT_L3  0x0F
// External aborts
#define ESR_DFSC_SYNC_EXT_ABORT     0x10 // Synchronous External Abort
#define ESR_DFSC_SYNC_EXT_ABORT_L0  0x14 // SEA on translation table walk, level 0
#define ESR_DFSC_SYNC_EXT_ABORT_L1  0x15 // SEA on translation table walk, level 1
#define ESR_DFSC_SYNC_EXT_ABORT_L2  0x16 // SEA on translation table walk, level 2
#define ESR_DFSC_SYNC_EXT_ABORT_L3  0x17 // SEA on translation table walk, level 3
// Alignment fault
#define ESR_DFSC_ALIGNMENT_FAULT  0x21

//
// Shell command line parameter definitions
//
SHELL_PARAM_ITEM  mMmioUtilParamList[] = {
  { L"-w", TypeValue },
  { L"-r", TypeValue },
  { L"-v", TypeFlag  },
  { L"-?", TypeFlag  },
  { NULL,  TypeMax   },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"MmioUtil";

//
// Exception handling state
//
STATIC EFI_CPU_ARCH_PROTOCOL      *mCpu                = NULL;
STATIC EFI_CPU_INTERRUPT_HANDLER  mOriginalSyncHandler = NULL;
STATIC volatile BOOLEAN           mInProtectedAccess   = FALSE;
STATIC volatile BOOLEAN           mExceptionOccurred   = FALSE;
STATIC volatile UINT64            mFaultAddress        = 0;
STATIC volatile UINT64            mExceptionSyndrome   = 0;

/**
  Check if ESR indicates a Data Abort that we should handle.

  @param[in] Esr  Exception Syndrome Register value.

  @retval TRUE    ESR indicates a Data Abort we should catch.
  @retval FALSE   Not a Data Abort.
**/
STATIC
BOOLEAN
IsDataAbort (
  IN UINT64  Esr
  )
{
  UINT32  Ec;

  Ec = (Esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

  //
  // Catch Data Aborts from current EL or lower EL, and SErrors
  //
  return (Ec == ESR_EC_DATA_ABORT) ||
         (Ec == ESR_EC_DATA_ABORT_EL0) ||
         (Ec == ESR_EC_SERROR);
}

/**
  Get a human-readable description of the fault type.

  @param[in] Esr  Exception Syndrome Register value.

  @return String describing the fault type.
**/
STATIC
CONST CHAR16 *
GetFaultDescription (
  IN UINT64  Esr
  )
{
  UINT32  Ec;
  UINT32  Dfsc;

  Ec   = (Esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
  Dfsc = Esr & ESR_DFSC_MASK;

  if (Ec == ESR_EC_SERROR) {
    return L"SError (asynchronous external abort)";
  }

  if ((Ec != ESR_EC_DATA_ABORT) && (Ec != ESR_EC_DATA_ABORT_EL0)) {
    return L"Unknown exception";
  }

  switch (Dfsc) {
    case ESR_DFSC_TRANS_FAULT_L0:
    case ESR_DFSC_TRANS_FAULT_L1:
    case ESR_DFSC_TRANS_FAULT_L2:
    case ESR_DFSC_TRANS_FAULT_L3:
      return L"Translation fault (address not mapped)";

    case ESR_DFSC_ACCESS_FLAG_L0:
    case ESR_DFSC_ACCESS_FLAG_L1:
    case ESR_DFSC_ACCESS_FLAG_L2:
    case ESR_DFSC_ACCESS_FLAG_L3:
      return L"Access flag fault";

    case ESR_DFSC_PERM_FAULT_L0:
    case ESR_DFSC_PERM_FAULT_L1:
    case ESR_DFSC_PERM_FAULT_L2:
    case ESR_DFSC_PERM_FAULT_L3:
      return L"Permission fault";

    case ESR_DFSC_SYNC_EXT_ABORT:
      return L"Synchronous external abort (bus error)";

    case ESR_DFSC_SYNC_EXT_ABORT_L0:
    case ESR_DFSC_SYNC_EXT_ABORT_L1:
    case ESR_DFSC_SYNC_EXT_ABORT_L2:
    case ESR_DFSC_SYNC_EXT_ABORT_L3:
      return L"External abort on page table walk";

    case ESR_DFSC_ALIGNMENT_FAULT:
      return L"Alignment fault";

    default:
      return L"Data abort";
  }
}

/**
  Custom exception handler to catch Data Aborts.

  If we're in a protected access region and a Data Abort occurs, we record the
  error and advance past the faulting instruction instead of crashing.

  @param[in]     ExceptionType  Type of exception.
  @param[in,out] SystemContext  CPU context at time of exception.
**/
STATIC
VOID
EFIAPI
MmioUtilExceptionHandler (
  IN     EFI_EXCEPTION_TYPE  ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  UINT64  Esr;
  UINT64  Far;
  UINT64  Elr;

  //
  // Get exception syndrome and fault address from system context
  // For AArch64, these are in the SystemContextAArch64 structure
  //
  Esr = SystemContext.SystemContextAArch64->ESR;
  Far = SystemContext.SystemContextAArch64->FAR;
  Elr = SystemContext.SystemContextAArch64->ELR;

  //
  // If we're in a protected access and this is a Data Abort, handle it gracefully
  //
  if (mInProtectedAccess && IsDataAbort (Esr)) {
    //
    // Record the exception details
    //
    mExceptionOccurred = TRUE;
    mFaultAddress      = Far;
    mExceptionSyndrome = Esr;

    //
    // Advance past the faulting instruction (4 bytes for AArch64)
    //
    SystemContext.SystemContextAArch64->ELR = Elr + 4;

    //
    // Return to continue execution after the faulting instruction
    //
    return;
  }

  //
  // Not our exception - call the original handler if it exists
  //
  if (mOriginalSyncHandler != NULL) {
    mOriginalSyncHandler (ExceptionType, SystemContext);
  } else {
    //
    // No original handler - we have to let it crash
    // This shouldn't happen if we properly saved the original handler
    //
    DEBUG ((DEBUG_ERROR, "MmioUtil: Unhandled exception, ESR=0x%lx FAR=0x%lx\n", Esr, Far));
    CpuDeadLoop ();
  }
}

/**
  Install exception handler for catching bus errors.

  @retval EFI_SUCCESS     Handler installed.
  @retval Others          Failed to install handler.
**/
STATIC
EFI_STATUS
InstallExceptionHandler (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mCpu == NULL) {
    Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "MmioUtil: Failed to locate CPU protocol: %r\n", Status));
      return Status;
    }
  }

  //
  // Register our handler for synchronous exceptions
  // Note: We can't easily get the "original" handler in UEFI, so we just
  // install ours. The original behavior for unhandled exceptions is typically
  // to deadloop or reset.
  //
  Status = mCpu->RegisterInterruptHandler (
                   mCpu,
                   EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS,
                   MmioUtilExceptionHandler
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "MmioUtil: Failed to register exception handler: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Uninstall our exception handler.
**/
STATIC
VOID
UninstallExceptionHandler (
  VOID
  )
{
  if (mCpu != NULL) {
    //
    // Unregister by passing NULL as handler
    //
    mCpu->RegisterInterruptHandler (
            mCpu,
            EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS,
            NULL
            );
  }
}

/**
  Ensure the memory region is mapped in GCD and MMU.

  This function checks if the address is in the GCD memory map.
  If not, it adds the region as MMIO with uncached attributes.

  @param[in]  BaseAddress   Base address to map (will be 4KB aligned).
  @param[in]  Size          Size of the region to access.
  @param[in]  Verbose       Print verbose messages.

  @retval EFI_SUCCESS       Region is now accessible.
  @retval Others            Failed to map the region.
**/
STATIC
EFI_STATUS
EnsureMemoryMapped (
  IN UINT64   BaseAddress,
  IN UINT64   Size,
  IN BOOLEAN  Verbose
  )
{
  EFI_STATUS                       Status;
  UINT64                           AlignedBase;
  UINT64                           AlignedSize;
  UINT64                           AlignedEnd;
  UINT64                           ScanLocation;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  MemorySpace;
  UINT64                           OverlapSize;
  UINT64                           BaseOffset;

  //
  // Align to 4KB page boundaries (ARM MMU requirement)
  //
  AlignedBase = BaseAddress & ~(SIZE_4KB - 1);
  BaseOffset  = BaseAddress - AlignedBase;

  //
  // Check for overflow before adding offset to size
  //
  if (Size > (MAX_UINT64 - BaseOffset)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
      mHiiHandle,
      mAppName
      );
    return EFI_INVALID_PARAMETER;
  }

  AlignedSize = Size + BaseOffset;

  //
  // Check for overflow before aligning (ALIGN_VALUE can add up to SIZE_4KB - 1)
  //
  if (AlignedSize > (MAX_UINT64 - SIZE_4KB + 1)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
      mHiiHandle,
      mAppName
      );
    return EFI_INVALID_PARAMETER;
  }

  AlignedSize = ALIGN_VALUE (AlignedSize, SIZE_4KB);

  //
  // Check for overflow before computing end address
  //
  if (AlignedBase > (MAX_UINT64 - AlignedSize)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
      mHiiHandle,
      mAppName
      );
    return EFI_INVALID_PARAMETER;
  }

  AlignedEnd = AlignedBase + AlignedSize;

  ScanLocation = AlignedBase;
  while (ScanLocation < AlignedEnd) {
    Status = gDS->GetMemorySpaceDescriptor (ScanLocation, &MemorySpace);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_GCD_ERROR),
        mHiiHandle,
        mAppName,
        ScanLocation,
        Status
        );
      return Status;
    }

    OverlapSize = MIN (MemorySpace.BaseAddress + MemorySpace.Length, AlignedEnd) - ScanLocation;

    if (MemorySpace.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      //
      // Address not in memory map - add it as MMIO
      //
      if (Verbose) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_MAPPING),
          mHiiHandle,
          ScanLocation,
          ScanLocation + OverlapSize - 1
          );
      }

      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeMemoryMappedIo,
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_ADD_MEMORY_ERROR),
          mHiiHandle,
          mAppName,
          ScanLocation,
          OverlapSize,
          Status
          );
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_SET_ATTR_ERROR),
          mHiiHandle,
          mAppName,
          ScanLocation,
          OverlapSize,
          Status
          );
        return Status;
      }
    } else if (Verbose) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_ALREADY_MAPPED),
        mHiiHandle,
        ScanLocation,
        ScanLocation + OverlapSize - 1
        );
    }

    ScanLocation += OverlapSize;
  }

  return EFI_SUCCESS;
}

/**
  Perform a protected MMIO read operation.

  This function performs the read within an exception-protected region.
  If a bus error occurs, it returns an error instead of crashing.

  @param[in]  Address   Physical address to read from.
  @param[in]  Width     Access width in bytes (1, 2, 4, or 8).
  @param[out] Value     Pointer to store the read value.

  @retval EFI_SUCCESS        Read successful.
  @retval EFI_DEVICE_ERROR   Bus error (SEA) occurred.
  @retval EFI_INVALID_PARAMETER Invalid width.
**/
STATIC
EFI_STATUS
ProtectedMmioRead (
  IN  UINT64  Address,
  IN  UINTN   Width,
  OUT UINT64  *Value
  )
{
  //
  // Validate output pointer
  //
  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Reset exception state
  //
  mExceptionOccurred = FALSE;
  mFaultAddress      = 0;
  mExceptionSyndrome = 0;

  //
  // Enter protected region
  //
  MemoryFence ();
  mInProtectedAccess = TRUE;
  MemoryFence ();

  //
  // Perform the read
  //
  switch (Width) {
    case 1:
      *Value = MmioRead8 ((UINTN)Address);
      break;
    case 2:
      *Value = MmioRead16 ((UINTN)Address);
      break;
    case 4:
      *Value = MmioRead32 ((UINTN)Address);
      break;
    case 8:
      *Value = MmioRead64 ((UINTN)Address);
      break;
    default:
      mInProtectedAccess = FALSE;
      return EFI_INVALID_PARAMETER;
  }

  //
  // Exit protected region
  //
  MemoryFence ();
  mInProtectedAccess = FALSE;
  MemoryFence ();

  //
  // Check if an exception occurred
  //
  if (mExceptionOccurred) {
    *Value = 0xFFFFFFFFFFFFFFFFULL;  // Return all 1s like hardware would for failed read
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Perform a protected MMIO write operation.

  This function performs the write within an exception-protected region.
  If a bus error occurs, it returns an error instead of crashing.

  @param[in] Address   Physical address to write to.
  @param[in] Width     Access width in bytes (1, 2, 4, or 8).
  @param[in] Value     Value to write.

  @retval EFI_SUCCESS        Write successful.
  @retval EFI_DEVICE_ERROR   Bus error (SEA) occurred.
  @retval EFI_INVALID_PARAMETER Invalid width.
**/
STATIC
EFI_STATUS
ProtectedMmioWrite (
  IN UINT64  Address,
  IN UINTN   Width,
  IN UINT64  Value
  )
{
  //
  // Reset exception state
  //
  mExceptionOccurred = FALSE;
  mFaultAddress      = 0;
  mExceptionSyndrome = 0;

  //
  // Enter protected region
  //
  MemoryFence ();
  mInProtectedAccess = TRUE;
  MemoryFence ();

  //
  // Perform the write
  //
  switch (Width) {
    case 1:
      MmioWrite8 ((UINTN)Address, (UINT8)Value);
      break;
    case 2:
      MmioWrite16 ((UINTN)Address, (UINT16)Value);
      break;
    case 4:
      MmioWrite32 ((UINTN)Address, (UINT32)Value);
      break;
    case 8:
      MmioWrite64 ((UINTN)Address, Value);
      break;
    default:
      mInProtectedAccess = FALSE;
      return EFI_INVALID_PARAMETER;
  }

  //
  // Exit protected region
  //
  MemoryFence ();
  mInProtectedAccess = FALSE;
  MemoryFence ();

  //
  // Check if an exception occurred
  //
  if (mExceptionOccurred) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Print a value with the appropriate format based on width.

  @param[in] Address   Address that was accessed.
  @param[in] Width     Access width in bytes.
  @param[in] Value     Value to print.
**/
STATIC
VOID
PrintValue (
  IN UINT64  Address,
  IN UINTN   Width,
  IN UINT64  Value
  )
{
  switch (Width) {
    case 1:
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_VALUE_8),
        mHiiHandle,
        Address,
        (UINT8)Value
        );
      break;
    case 2:
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_VALUE_16),
        mHiiHandle,
        Address,
        (UINT16)Value
        );
      break;
    case 4:
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_VALUE_32),
        mHiiHandle,
        Address,
        (UINT32)Value
        );
      break;
    case 8:
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_VALUE_64),
        mHiiHandle,
        Address,
        Value
        );
      break;
  }
}

/**
  Application entry point.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS       Operation completed successfully.
  @retval Others            An error occurred.
**/
EFI_STATUS
EFIAPI
InitializeMmioUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  LIST_ENTRY                   *ParamPackage;
  CHAR16                       *ProblemParam;
  CONST CHAR16                 *ValueStr;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  UINT64   Address;
  UINT64   Value;
  UINTN    Width;
  UINTN    RepeatCount;
  BOOLEAN  HasValue;
  BOOLEAN  Verbose;
  UINTN    Index;
  UINTN    ParamCount;
  BOOLEAN  ExceptionHandlerInstalled;

  Width                     = 4;      // Default to 32-bit access
  RepeatCount               = 1;      // Default to single access
  HasValue                  = FALSE;
  Verbose                   = FALSE;
  ExceptionHandlerInstalled = FALSE;

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
  // Publish HII package list to HII Database
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

  //
  // Install exception handler for catching bus errors
  //
  Status = InstallExceptionHandler ();
  if (!EFI_ERROR (Status)) {
    ExceptionHandlerInstalled = TRUE;
  }

  //
  // Parse command line parameters
  //
  Status = ShellCommandLineParseEx (mMmioUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_UNKNOWN_PARAM),
      mHiiHandle,
      mAppName,
      ProblemParam
      );
    goto Done;
  }

  //
  // Check for help request
  //
  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_MMIOUTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  //
  // Get verbose flag
  //
  Verbose = ShellCommandLineGetFlag (ParamPackage, L"-v");

  //
  // Get access width
  //
  ValueStr = ShellCommandLineGetValue (ParamPackage, L"-w");
  if (ValueStr != NULL) {
    Width = ShellStrToUintn (ValueStr);
    if ((Width != 1) && (Width != 2) && (Width != 4) && (Width != 8)) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_INVALID_WIDTH),
        mHiiHandle,
        mAppName
        );
      goto Done;
    }
  }

  //
  // Get repeat count
  //
  ValueStr = ShellCommandLineGetValue (ParamPackage, L"-r");
  if (ValueStr != NULL) {
    RepeatCount = ShellStrToUintn (ValueStr);
    if (RepeatCount == 0) {
      RepeatCount = 1;
    }

    //
    // Sanity check: limit repeat count to prevent overflow in Width * RepeatCount
    // Max reasonable value: 1MB / min_width = 1MB / 1 = 1M iterations
    //
    if (RepeatCount > 0x100000) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
        mHiiHandle,
        mAppName
        );
      goto Done;
    }
  }

  //
  // Get positional parameters (address and optional value)
  //
  ParamCount = ShellCommandLineGetCount (ParamPackage);
  if (ParamCount < 2) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_NO_ADDRESS),
      mHiiHandle,
      mAppName
      );
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_MMIOUTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  //
  // Parse address
  //
  ValueStr = ShellCommandLineGetRawValue (ParamPackage, 1);
  if (ValueStr == NULL) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_NO_ADDRESS),
      mHiiHandle,
      mAppName
      );
    goto Done;
  }

  Status = ShellConvertStringToUint64 (ValueStr, &Address, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_INVALID_ADDRESS),
      mHiiHandle,
      mAppName,
      ValueStr
      );
    goto Done;
  }

  //
  // Check address alignment
  //
  if ((Address & (Width - 1)) != 0) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_ALIGNMENT_ERROR),
      mHiiHandle,
      mAppName,
      Address,
      Width
      );
    goto Done;
  }

  //
  // Parse optional value (for write operation)
  //
  if (ParamCount >= 3) {
    ValueStr = ShellCommandLineGetRawValue (ParamPackage, 2);
    if (ValueStr != NULL) {
      Status = ShellConvertStringToUint64 (ValueStr, &Value, TRUE, FALSE);
      if (EFI_ERROR (Status)) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_INVALID_VALUE),
          mHiiHandle,
          mAppName,
          ValueStr
          );
        goto Done;
      }

      HasValue = TRUE;
    }
  }

  //
  // Check for overflow in total access size (Width * RepeatCount)
  //
  if (RepeatCount > (MAX_UINT64 / Width)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
      mHiiHandle,
      mAppName
      );
    goto Done;
  }

  //
  // Check for address overflow (Address + Width * RepeatCount)
  //
  if (Address > (MAX_UINT64 - (Width * RepeatCount))) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_MMIOUTIL_SIZE_OVERFLOW),
      mHiiHandle,
      mAppName
      );
    goto Done;
  }

  //
  // Ensure memory region is mapped
  //
  Status = EnsureMemoryMapped (Address, Width * RepeatCount, Verbose);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Perform the access
  //
  if (HasValue) {
    //
    // Write operation
    //
    Status = ProtectedMmioWrite (Address, Width, Value);
    if (Status == EFI_DEVICE_ERROR) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_BUS_ERROR),
        mHiiHandle,
        mAppName,
        GetFaultDescription (mExceptionSyndrome),
        Address,
        mExceptionSyndrome
        );
      goto Done;
    } else if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_WRITE_ERROR),
        mHiiHandle,
        mAppName,
        Address,
        Status
        );
      goto Done;
    }

    if (Verbose) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_WRITE_SUCCESS),
        mHiiHandle,
        Address,
        Value
        );
    }

    //
    // Read back and display
    //
    Status = ProtectedMmioRead (Address, Width, &Value);
    if (Status == EFI_DEVICE_ERROR) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_MMIOUTIL_BUS_ERROR),
        mHiiHandle,
        mAppName,
        GetFaultDescription (mExceptionSyndrome),
        Address,
        mExceptionSyndrome
        );
    } else if (!EFI_ERROR (Status)) {
      PrintValue (Address, Width, Value);
    }
  } else {
    //
    // Read operation (possibly repeated for a range)
    //
    for (Index = 0; Index < RepeatCount; Index++) {
      UINT64  CurrentAddress = Address + (Index * Width);

      Status = ProtectedMmioRead (CurrentAddress, Width, &Value);
      if (Status == EFI_DEVICE_ERROR) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_BUS_ERROR),
          mHiiHandle,
          mAppName,
          GetFaultDescription (mExceptionSyndrome),
          CurrentAddress,
          mExceptionSyndrome
          );
        break;
      } else if (EFI_ERROR (Status)) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_MMIOUTIL_READ_ERROR),
          mHiiHandle,
          mAppName,
          CurrentAddress,
          Status
          );
        break;
      }

      PrintValue (CurrentAddress, Width, Value);
    }
  }

Done:
  //
  // Uninstall exception handler
  //
  if (ExceptionHandlerInstalled) {
    UninstallExceptionHandler ();
  }

  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
