/** @file

  HspDoorbell private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HSP_DOORBELL_PRIVATE_H__
#define __HSP_DOORBELL_PRIVATE_H__

#include <Library/DeviceDiscoveryLib.h>

#define HSP_DIMENSIONING  0x380

#define HSP_DB_REG_TRIGGER  0x0
#define HSP_DB_REG_ENABLE   0x4
#define HSP_DB_REG_RAW      0x8
#define HSP_DB_REG_PENDING  0xc

#define HSP_COMMON_REGION_SIZE    SIZE_64KB
#define HSP_DOORBELL_REGION_SIZE  0x100

#define HSP_MASTER_SECURE_CCPLEX  1
#define HSP_MASTER_CCPLEX         17
#define HSP_MASTER_DPMU           18
#define HSP_MASTER_BPMP           19
#define HSP_MASTER_SPE            20
#define HSP_MASTER_SCE            21
#define HSP_MASTER_APE            27

typedef enum {
  HspDoorbellDpmu,
  HspDoorbellCcplex,
  HspDoorbellCcplexTz,
  HspDoorbellBpmp,
  HspDoorbellSpe,
  HspDoorbellSce,
  HspDoorbellApe,
  HspDoorbellMax
} HSP_DOORBELL_ID;

typedef UINT32 HSP_MASTER_ID;

typedef struct {
  union {
    UINT32    RawValue;
    struct {
      UINT8    SharedMailboxes      : 4;
      UINT8    SharedSemaphores     : 4;
      UINT8    ArbitratedSemaphores : 4;
      UINT8    Reserved             : 4;
    };
  };
} HSP_DIMENSIONING_DATA;

#define HSP_MAILBOX_SHIFT_SIZE    15
#define HSP_SEMAPHORE_SHIFT_SIZE  16

//
// HspDoorbell driver private data structure
//

#define HSP_DOORBELL_SIGNATURE  SIGNATURE_32('H','S','P','D')

typedef struct {
  //
  // Standard signature used to identify HspDoorbell private data
  //
  UINT32                  Signature;

  //
  // Array of the doorbell locations
  EFI_PHYSICAL_ADDRESS    DoorbellLocation[HspDoorbellMax];
} NVIDIA_HSP_DOORBELL_PRIVATE_DATA;

/**
  This function allows for a remote IPC to the BPMP firmware to be executed.

  @param[in]     DoorbellLocation    A pointer to HSP Doorbell address.
  @param[in]     Doorbell            Doorbell to ring

  @return EFI_SUCCESS               The doorbell has been rung.
  @return EFI_UNSUPPORTED           The doorbell is not supported.
  @return EFI_DEVICE_ERROR          Failed to ring the doorbell.
  @return EFI_NOT_READY             Doorbell is not ready to receive from CCPLEX
**/
EFI_STATUS
HspDoorbellRingDoorbell (
  IN  EFI_PHYSICAL_ADDRESS  *DoorbellLocation,
  IN  HSP_DOORBELL_ID       Doorbell
  );

/**
  This function enables the channel for communication with the CCPLEX.

  @param[in]     DoorbellLocation    A pointer to HSP Doorbell address.
  @param[in]     Doorbell            Doorbell of the channel to enable

  @return EFI_SUCCESS               The channel is enabled.
  @return EFI_UNSUPPORTED           The channel is not supported.
  @return EFI_DEVICE_ERROR          Failed to enable channel.
**/
EFI_STATUS
HspDoorbellEnableChannel (
  IN  EFI_PHYSICAL_ADDRESS  *DoorbellLocation,
  IN  HSP_DOORBELL_ID       Doorbell
  );

/**
  This routine initializes HSP Doorbell on the device..

  @param DTNodeInfo          Info regarding HSP device tree base address,node, offset,
                              device type and init function.
  @param HspDevice           A pointer to the HspDevice.
  @param DoorbellLocation    A pointer to HSP Doorbell address.

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
HspDoorbellInit (
  IN      NVIDIA_DT_NODE_INFO      *DTNodeInfo,
  IN      NON_DISCOVERABLE_DEVICE  *HspDevice,
  IN OUT  EFI_PHYSICAL_ADDRESS     *DoorbellLocation
  );

#endif
