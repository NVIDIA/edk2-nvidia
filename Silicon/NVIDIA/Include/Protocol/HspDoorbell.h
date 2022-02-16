/** @file
  NVIDIA Hardware Synchronization Primative (HSP) Doorbell Protocol

  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HSP_DOORBELL_PROTOCOL_H__
#define __HSP_DOORBELL_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_HSP_DOORBELL_PROTOCOL_GUID \
  { \
  0xe72494c2, 0xdb40, 0x4c06, { 0xbe, 0x76, 0xdc, 0x53, 0x01, 0x2f, 0x2c, 0x59 } \
  }

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

//
// Define for forward reference.
//
typedef struct _NVIDIA_HSP_DOORBELL_PROTOCOL NVIDIA_HSP_DOORBELL_PROTOCOL;

/**
  This rings the specified doorbell.

  @param[in]     This                The instance of the NVIDIA_HSP_DOORBELL_PROTOCOL.
  @param[in]     Doorbell            Doorbell to ring

  @return EFI_SUCCESS               The doorbell has been rung.
  @return EFI_UNSUPPORTED           The doorbell is not supported.
  @return EFI_DEVICE_ERROR          Failed to ring the doorbell.
  @return EFI_NOT_READY             Doorbell is not ready to receive from CCPLEX
**/
typedef
EFI_STATUS
(EFIAPI *HSP_DOORBELL_RING_DOORBELL) (
  IN  NVIDIA_HSP_DOORBELL_PROTOCOL   *This,
  IN  HSP_DOORBELL_ID                Doorbell
  );

/**
  This function enables the channel for communication with the CCPLEX.

  @param[in]     This                The instance of the NVIDIA_HSP_DOORBELL_PROTOCOL.
  @param[in]     Doorbell            Doorbell of the channel to enable

  @return EFI_SUCCESS               The channel is enabled.
  @return EFI_UNSUPPORTED           The channel is not supported.
  @return EFI_DEVICE_ERROR          Failed to enable channel.
**/
typedef
EFI_STATUS
(EFIAPI *HSP_DOORBELL_ENABLE_CHANNEL) (
  IN  NVIDIA_HSP_DOORBELL_PROTOCOL   *This,
  IN  HSP_DOORBELL_ID                Doorbell
  );


/// NVIDIA_BPMP_IPC_PROTOCOL protocol structure.
struct _NVIDIA_HSP_DOORBELL_PROTOCOL {

  HSP_DOORBELL_RING_DOORBELL  RingDoorbell;
  HSP_DOORBELL_ENABLE_CHANNEL EnableChannel;

};

extern EFI_GUID gNVIDIAHspDoorbellProtocolGuid;

#endif
