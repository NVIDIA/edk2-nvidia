/** @file

  Copyright (c) 2020 - 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2012-2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef DWEMAC_SNP_DXE_H__
#define DWEMAC_SNP_DXE_H__

// Protocols used by this driver
#include <Protocol/SimpleNetwork.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DevicePath.h>
#include <Protocol/NonDiscoverableDevice.h>

#include <Library/UefiLib.h>

#include "PhyDxeUtil.h"
#include "EmacDxeUtil.h"

/*------------------------------------------------------------------------------
  Information Structure
------------------------------------------------------------------------------*/

typedef struct {
  MAC_ADDR_DEVICE_PATH                   MacAddrDP;
  EFI_DEVICE_PATH_PROTOCOL               End;
} SIMPLE_NETWORK_DEVICE_PATH;

typedef struct {
  // Driver signature
  UINT32                                 Signature;
  EFI_HANDLE                             ControllerHandle;

  // EFI SNP protocol instances
  EFI_SIMPLE_NETWORK_PROTOCOL            Snp;
  EFI_SIMPLE_NETWORK_MODE                SnpMode;

  EMAC_DRIVER                            MacDriver;
  PHY_DRIVER                             PhyDriver;

  EFI_LOCK                               Lock;

  UINTN                                  MacBase;
  UINT32                                 NumMacs;

  EFI_PHYSICAL_ADDRESS                   MaxAddress;

  BOOLEAN                                BroadcastEnabled;
  UINT32                                 MulticastFiltersEnabled;

  EFI_EVENT                              DeviceTreeNotifyEvent;
  EFI_EVENT                              AcpiNotifyEvent;
  EFI_EVENT                              ExitBootServiceEvent;
  CHAR8                                  DeviceTreePath[64];
} SIMPLE_NETWORK_DRIVER;

extern EFI_COMPONENT_NAME_PROTOCOL       gSnpComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL      gSnpComponentName2;

#define SNP_DRIVER_SIGNATURE             SIGNATURE_32('A', 'S', 'N', 'P')
#define INSTANCE_FROM_SNP_THIS(a)        CR(a, SIMPLE_NETWORK_DRIVER, Snp, SNP_DRIVER_SIGNATURE)

#define ETHERNET_MAC_ADDRESS_INDEX                                 0
#define ETHERNET_MAC_BROADCAST_INDEX                               1
#define ETHERNET_MAC_MULTICAST_INDEX                               2

/*---------------------------------------------------------------------------------------------------------------------

  UEFI-Compliant functions for EFI_SIMPLE_NETWORK_PROTOCOL

  Refer to the Simple Network Protocol section (24.1) in the UEFI 2.8 Specification for related definitions

---------------------------------------------------------------------------------------------------------------------*/

EFI_STATUS
EFIAPI
SnpStart (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp
  );

EFI_STATUS
EFIAPI
SnpStop (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp
  );

EFI_STATUS
EFIAPI
SnpInitialize (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       UINTN                       ExtraRxBufferSize OPTIONAL,
  IN       UINTN                       ExtraTxBufferSize OPTIONAL
  );

EFI_STATUS
EFIAPI
SnpReset (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       BOOLEAN                     ExtendedVerification
  );

EFI_STATUS
EFIAPI
SnpShutdown (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp
  );

EFI_STATUS
EFIAPI
SnpReceiveFilters (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       UINT32                      Enable,
  IN       UINT32                      Disable,
  IN       BOOLEAN                     ResetMCastFilter,
  IN       UINTN                       MCastFilterCnt  OPTIONAL,
  IN       EFI_MAC_ADDRESS             *MCastFilter  OPTIONAL
  );

EFI_STATUS
EFIAPI
SnpStationAddress (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       BOOLEAN                     Reset,
  IN       EFI_MAC_ADDRESS             *NewMac
);

EFI_STATUS
EFIAPI
SnpStatistics (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       BOOLEAN                     Reset,
  IN  OUT  UINTN                       *StatSize,
      OUT  EFI_NETWORK_STATISTICS      *Statistics
  );

EFI_STATUS
EFIAPI
SnpMcastIptoMac (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       BOOLEAN                     IsIpv6,
  IN       EFI_IP_ADDRESS              *Ip,
      OUT  EFI_MAC_ADDRESS             *McastMac
  );

EFI_STATUS
EFIAPI
SnpNvData (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       BOOLEAN                     ReadWrite,
  IN       UINTN                       Offset,
  IN       UINTN                       BufferSize,
  IN  OUT  VOID                        *Buffer
  );

EFI_STATUS
EFIAPI
SnpGetStatus (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  OUT      UINT32                      *IrqStat  OPTIONAL,
  OUT      VOID                        **TxBuff  OPTIONAL
  );

EFI_STATUS
EFIAPI
SnpTransmit (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
  IN       UINTN                       HdrSize,
  IN       UINTN                       BuffSize,
  IN       VOID                        *Data,
  IN       EFI_MAC_ADDRESS             *SrcAddr  OPTIONAL,
  IN       EFI_MAC_ADDRESS             *DstAddr  OPTIONAL,
  IN       UINT16                      *Protocol OPTIONAL
  );

EFI_STATUS
EFIAPI
SnpReceive (
  IN       EFI_SIMPLE_NETWORK_PROTOCOL *Snp,
      OUT  UINTN                       *HdrSize      OPTIONAL,
  IN  OUT  UINTN                       *BuffSize,
      OUT  VOID                        *Data,
      OUT  EFI_MAC_ADDRESS             *SrcAddr      OPTIONAL,
      OUT  EFI_MAC_ADDRESS             *DstAddr      OPTIONAL,
      OUT  UINT16                      *Protocol     OPTIONAL
  );

//Internal helper functions
/**
  This function commits the current filters to the OSI layer

  @param Snp              A pointer to the SIMPLE_NETWORK_DRIVER instance.
  @param UpdateMac        Boolean that indicates if the MAC address may have changed
  @param UpdateMCast      Boolean that indicates the multicast list may have changed.

  @retval EFI_SUCCESS            The receive filter settings wer updated.

**/
EFI_STATUS
EFIAPI
SnpCommitFilters (
  IN SIMPLE_NETWORK_DRIVER *Snp,
  IN BOOLEAN               UpdateMac,
  IN BOOLEAN               UpdateMCast
  );

#endif // DWEMAC_SNP_DXE_H__
