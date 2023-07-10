/** @file

  MCE ARI library

  Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <ArmMpidr.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MceAriLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TimerLib.h>

#define BIT(Number)  (1 << (Number))

// ARI Version numbers
#define TEGRA_ARI_VERSION_MAJOR  8
#define TEGRA_ARI_VERSION_MINOR  1

// ARI Request IDs
#define TEGRA_ARI_VERSION_CMD    0
#define TEGRA_ARI_ECHO_CMD       1
#define TEGRA_ARI_NUM_CORES_CMD  2

// Register offsets for ARI request/results
#define ARI_REQUEST_OFFS             0x00
#define ARI_REQUEST_EVENT_MASK_OFFS  0x08
#define ARI_STATUS_OFFS              0x10
#define ARI_REQUEST_DATA_LO_OFFS     0x18
#define ARI_REQUEST_DATA_HI_OFFS     0x20
#define ARI_RESPONSE_DATA_LO_OFFS    0x28
#define ARI_RESPONSE_DATA_HI_OFFS    0x30

// Status values for the current request
#define ARI_REQ_PENDING  1
#define ARI_REQ_ONGOING  2

// Request completion status values
#define ARI_REQ_ERROR_STATUS_MASK   0xFC
#define ARI_REQ_ERROR_STATUS_SHIFT  2
#define ARI_REQ_NO_ERROR            0
#define ARI_REQ_REQUEST_KILLED      1
#define ARI_REQ_NS_ERROR            2
#define ARI_REQ_EXECUTION_ERROR     0x3F

// Software request completion status values
#define ARI_REQ_TIMEOUT         0x100U
#define ARI_REQ_BAD_EVENT_MASK  0x200U

// Request control bits
#define ARI_REQUEST_VALID_BIT  (1U << 8)
#define ARI_REQUEST_KILL_BIT   (1U << 9)
#define ARI_REQUEST_NS_BIT     (1U << 31)

// Default timeout to wait for ARI completion
#define ARI_MAX_RETRY_US  2000000U

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_CPUS               (PLATFORM_MAX_CLUSTERS * \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

/**
  Returns flag indicating execution environment support for the MCE ARI interface.

  @return       BOOLEAN         TRUE if MCE ARI is supported
**/
STATIC
BOOLEAN
EFIAPI
MceAriSupported (
  VOID
  )
{
  if (TegraGetPlatform () == TEGRA_PLATFORM_VDK) {
    return FALSE;
  }

  return TRUE;
}

/**
  Reads an ARI interface register

  @param[in]    AriBase         ARI register aperture base address
  @param[in]    Register        ARI register offset

  @return       UINT32          Contents of requested register
**/
STATIC
UINT32
EFIAPI
AriRead32 (
  IN  UINTN   AriBase,
  IN  UINT32  Register
  )
{
  UINT32  Value;

  if (MceAriSupported ()) {
    Value = MmioRead32 (AriBase + Register);
  } else {
    // force bad status in AriRequestWait ()
    Value = ARI_REQ_ERROR_STATUS_MASK;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: AriBase=0x%x, Register=0x%x, Value=0x%x\n",
    __FUNCTION__,
    AriBase,
    Register,
    Value
    ));

  return Value;
}

/**
  Writes an ARI interface register

  @param[in]    AriBase         ARI register aperture base address
  @param[in]    Value           Value to write to the register
  @param[in]    Register        ARI register offset

  @return       VOID
**/
STATIC
VOID
EFIAPI
AriWrite32 (
  IN  UINTN   AriBase,
  IN  UINT32  Value,
  IN  UINT32  Register
  )
{
  DEBUG ((
    DEBUG_VERBOSE,
    "%a: AriBase=0x%x, Register=0x%x, Value=0x%x\n",
    __FUNCTION__,
    AriBase,
    Register,
    Value
    ));

  if (MceAriSupported ()) {
    MmioWrite32 (AriBase + Register, Value);
  }
}

/**
  Returns the ARI_RESPONSE_DATA_LO register value

  @param[in]    AriBase         ARI register aperture base address

  @return       UINT32          ARI_RESPONSE_DATA_LO value
**/
STATIC
UINT32
EFIAPI
AriGetResponseLow (
  IN  UINTN  AriBase
  )
{
  return AriRead32 (AriBase, ARI_RESPONSE_DATA_LO_OFFS);
}

/**
  Returns the ARI_RESPONSE_DATA_HI register value

  @param[in]    AriBase         ARI register aperture base address

  @return       UINT32          ARI_RESPONSE_DATA_HI value
**/
STATIC
UINT32
EFIAPI
AriGetResponseHigh (
  IN  UINTN  AriBase
  )
{
  return AriRead32 (AriBase, ARI_RESPONSE_DATA_HI_OFFS);
}

/**
  Clobber the ARI response registers, required before starting a new request.

  @param[in]    AriBase         ARI register aperture base address

  @return       VOID
**/
STATIC
VOID
EFIAPI
AriClobberResponse (
  IN  UINTN  AriBase
  )
{
  AriWrite32 (AriBase, 0, ARI_RESPONSE_DATA_LO_OFFS);
  AriWrite32 (AriBase, 0, ARI_RESPONSE_DATA_HI_OFFS);
}

/**
  Send an ARI request

  @param[in]    AriBase         ARI register aperture base address
  @param[in]    EventMask       Event mask for the request
  @param[in]    Request         Request code
  @param[in]    Lo              Low word of data for the request
  @param[in]    Hi              High word of data for the request

  @return       VOID
**/
STATIC
VOID
EFIAPI
AriSendRequest (
  IN  UINTN   AriBase,
  IN  UINT32  EventMask,
  IN  UINT32  Request,
  IN  UINT32  Lo,
  IN  UINT32  Hi
  )
{
  AriWrite32 (AriBase, Lo, ARI_REQUEST_DATA_LO_OFFS);
  AriWrite32 (AriBase, Hi, ARI_REQUEST_DATA_HI_OFFS);
  AriWrite32 (AriBase, EventMask, ARI_REQUEST_EVENT_MASK_OFFS);
  AriWrite32 (AriBase, (Request | ARI_REQUEST_VALID_BIT), ARI_REQUEST_OFFS);
}

/**
  Send an ARI request and wait for completion for up to ARI_MAX_RETRY_US us.

  @param[in]    AriBase         ARI register aperture base address
  @param[in]    EventMask       Event mask for the request. Must be 0.
  @param[in]    Request         Request code
  @param[in]    Lo              Low word of data for the request
  @param[in]    Hi              High word of data for the request

  @return       ARI_REQ_NO_ERROR        Request completed successfully
  @return       ARI_REQ_REQUEST_KILLED  Request was killed
  @return       ARI_REQ_NS_ERROR        Request had an NS error
  @return       ARI_REQ_EXECUTION_ERROR Request had an execution error
  @return       ARI_REQ_TIMEOUT         Request timed out
  @return       ARI_REQ_BAD_EVENT_MASK  Unsupported EventMask parameter
**/
STATIC
UINT32
EFIAPI
AriRequestWait (
  IN  UINTN   AriBase,
  IN  UINT32  EventMask,
  IN  UINT32  Request,
  IN  UINT32  Lo,
  IN  UINT32  Hi
  )
{
  UINT32  Retries;
  UINT32  Status;

  // For each ARI command, the registers that are not used are listed
  // as "Must be set to 0" and  MCE firmware enforces a check for it.
  // So, clear response lo/hi data before sending out command.
  AriClobberResponse (AriBase);

  // send the request
  AriSendRequest (AriBase, EventMask, Request, Lo, Hi);

  // if no SW event trigger for the request, poll for completion with timeout
  if (EventMask == 0) {
    Retries = ARI_MAX_RETRY_US;
    while (Retries != 0) {
      Status = AriRead32 (AriBase, ARI_STATUS_OFFS);
      if ((Status & (ARI_REQ_ONGOING | ARI_REQ_PENDING |
                     ARI_REQ_ERROR_STATUS_MASK)) == 0)
      {
        break;
      }

      // return on error
      if ((Status & ARI_REQ_ERROR_STATUS_MASK) != 0) {
        UINT32  ErrorStatus;
        ErrorStatus = (Status & ARI_REQ_ERROR_STATUS_MASK) >>
                      ARI_REQ_ERROR_STATUS_SHIFT;
        DEBUG ((DEBUG_INFO, "ARI request got error: 0x%x\n", ErrorStatus));
        return ErrorStatus;
      }

      // delay and continue polling
      MicroSecondDelay (1);
      Retries--;
    }

    // timeout error
    if (Retries == 0) {
      DEBUG ((DEBUG_ERROR, "ARI request timed out: Request=%d\n", Request));
      return ARI_REQ_TIMEOUT;
    }
  } else {
    // non-zero EventMask not supported
    ASSERT (EventMask == 0);
    return ARI_REQ_BAD_EVENT_MASK;
  }

  return ARI_REQ_NO_ERROR;
}

/**
  Returns the MCE ARI interface version.

  @param[in]    AriBase         ARI register aperture base address

  @return       UINT64          ARI Version: [63:32] Major version,
                                              [31:0] Minor version.
**/
STATIC
UINT64
EFIAPI
AriGetVersion (
  IN  UINTN  AriBase
  )
{
  UINT32  Status;
  UINT64  Version;

  Status = AriRequestWait (AriBase, 0, TEGRA_ARI_VERSION_CMD, 0, 0);

  if (Status == ARI_REQ_NO_ERROR) {
    Version  = AriGetResponseLow (AriBase);
    Version |= (((UINT64)AriGetResponseHigh (AriBase)) << 32);
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARI request failed, returning version=0!\n",
      __FUNCTION__
      ));
    Version = 0;
  }

  return Version;
}

/**
  Returns a bitmask of enabled cores

  @param[in]    AriBase         ARI register aperture base address

  @return       UINT32          [15:0]: bit mask indicating which cores on
                                        the ccplex are enabled.  Each bit
                                        corresponds to a Linear Core ID.
**/
STATIC
UINT32
EFIAPI
AriGetCoresEnabledBitMask (
  IN  UINTN  AriBase
  )
{
  UINT32  Status;
  UINT32  CoreBitMask;

  Status = AriRequestWait (AriBase, 0, TEGRA_ARI_NUM_CORES_CMD, 0, 0);

  if (Status == ARI_REQ_NO_ERROR) {
    CoreBitMask = AriGetResponseLow (AriBase);
  } else {
    if (MceAriSupported ()) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ARI request fail, returning core 0 only!\n",
        __FUNCTION__
        ));
    }

    CoreBitMask = 0x1;
  }

  return CoreBitMask & 0xFFFFU;
}

/**
  Returns the Linear Core ID for a given MPIDR

  @param[in]    Mpidr           Mpidr register value

  @return       UINTN           Linear Core ID
**/
STATIC
UINTN
EFIAPI
MceAriMpidrToLinearCoreId (
  UINT64  Mpidr
  )
{
  UINTN  Cluster;
  UINTN  Core;
  UINTN  LinearCoreId;

  Cluster = (Mpidr >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK;
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  Core = (Mpidr >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK;
  ASSERT (Core < PLATFORM_MAX_CORES_PER_CLUSTER);

  LinearCoreId = (Cluster * PLATFORM_MAX_CORES_PER_CLUSTER) + Core;

  DEBUG ((
    DEBUG_INFO,
    "%a: Mpidr=0x%llx Cluster=%u, Core=%u, LinearCoreId=%u\n",
    __FUNCTION__,
    Mpidr,
    Cluster,
    Core,
    LinearCoreId
    ));

  return LinearCoreId;
}

/**
  Returns the Linear Core ID for the currently executing core

  @return       UINTN           Linear Core ID
**/
STATIC
UINTN
EFIAPI
MceAriGetCurrentLinearCoreId (
  VOID
  )
{
  UINT64  Mpidr;
  UINTN   LinearCoreId;

  Mpidr        = ArmReadMpidr ();
  LinearCoreId = MceAriMpidrToLinearCoreId (Mpidr);

  return LinearCoreId;
}

/**
  Returns the ARI register aperture base address for the currently executing core.

  @return       UINTN           ARI register aperture base address
**/
STATIC
UINTN
EFIAPI
MceAriGetApertureBase (
  VOID
  )
{
  UINTN   LinearCoreId;
  UINT32  ApertureOffset;

  LinearCoreId   = MceAriGetCurrentLinearCoreId ();
  ApertureOffset = MCE_ARI_APERTURE_OFFSET (LinearCoreId);

  return FixedPcdGet64 (PcdTegraMceAriApertureBaseAddress) + ApertureOffset;
}

UINT64
EFIAPI
MceAriGetVersion (
  VOID
  )
{
  UINTN  AriBase;

  AriBase = MceAriGetApertureBase ();
  return AriGetVersion (AriBase);
}

EFI_STATUS
EFIAPI
MceAriCheckCoreEnabled (
  IN UINT64  *Mpidr
  )
{
  UINTN   AriBase;
  UINTN   LinearCoreId;
  UINT32  LinearCoreIdBitmap;

  LinearCoreId = MceAriMpidrToLinearCoreId (*Mpidr);
  ASSERT (LinearCoreId < PLATFORM_MAX_CPUS);

  AriBase            = MceAriGetApertureBase ();
  LinearCoreIdBitmap = AriGetCoresEnabledBitMask (AriBase);
  if (!(LinearCoreIdBitmap & BIT (LinearCoreId))) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Fills in bit map of enabled cores

**/
EFI_STATUS
EFIAPI
MceAriGetEnabledCoresBitMap (
  IN  UINT64  *EnabledCoresBitMap
  )
{
  UINTN  AriBase;

  ASSERT (PLATFORM_MAX_CPUS <= 64);

  AriBase               = MceAriGetApertureBase ();
  EnabledCoresBitMap[0] = AriGetCoresEnabledBitMask (AriBase);

  return EFI_SUCCESS;
}
