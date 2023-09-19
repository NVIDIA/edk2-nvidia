/** @file
  NVIDIA QSPI Controller Protocol

  SPDX-FileCopyrightText: Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_QSPI_CONTROLLER_PROTOCOL_H__
#define __NVIDIA_QSPI_CONTROLLER_PROTOCOL_H__

#include <Library/QspiControllerLib.h>

#define NVIDIA_QSPI_CONTROLLER_PROTOCOL_GUID \
  { \
  0x01458542, 0x64b6, 0x42d9, { 0x80, 0x8c, 0x54, 0x42, 0x54, 0xd8, 0x8f, 0xc6 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_QSPI_CONTROLLER_PROTOCOL NVIDIA_QSPI_CONTROLLER_PROTOCOL;

/**
  Device specific features
**/
typedef enum QspiDevFeature {
  QspiDevFeatUnknown,      ///< 0 - Unknown feature
  QspiDevFeatWaitStateEn,  ///< 1 - Enable wait state
  QspiDevFeatWaitStateDis, ///< 2 - Disable wait state
  QspiDevFeatMax
} QSPI_DEV_FEATURE;

/**
  Perform a single transaction on QSPI bus.

  @param[in] This                  Instance of protocol
  @param[in] Packet                Transaction context

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *QSPI_CONTROLLER_PERFORM_TRANSACTION)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN QSPI_TRANSACTION_PACKET         *Packet
  );

/**
  Get QSPI clock speed.

  @param[in] This                  Instance of protocol
  @param[in] ClockSpeed            Pointer to get clock speed

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *QSPI_CONTROLLER_GET_CLOCK_SPEED)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN UINT64                          *ClockSpeed
  );

/**
  Set QSPI clock speed.

  @param[in] This                  Instance of protocol
  @param[in] ClockSpeed            Clock speed

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *QSPI_CONTROLLER_SET_CLOCK_SPEED)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN UINT64                          ClockSpeed
  );

/**
  Get QSPI number of chip selects

  @param[in]  This                  Instance of protocol
  @param[out] NumChipSelects        Pointer to store number of chip selects

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *QSPI_CONTROLLER_GET_NUM_CHIP_SELECTS)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  OUT UINT8                          *NumChipSelects
  );

/**
  Apply QSPI controller settings for a specific device

  @param[in] This                  Instance of protocol
  @param[in] DeviceFeature         Device feature to initialize

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *QSPI_CONTROLLER_APPLY_DEVICE_SPECIFIC_SETTINGS)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN QSPI_DEV_FEATURE                DeviceFeature
  );

/// NVIDIA_QSPI_CONTROLLER_PROTOCOL protocol structure.
struct _NVIDIA_QSPI_CONTROLLER_PROTOCOL {
  QSPI_CONTROLLER_PERFORM_TRANSACTION               PerformTransaction;
  QSPI_CONTROLLER_GET_CLOCK_SPEED                   GetClockSpeed;
  QSPI_CONTROLLER_SET_CLOCK_SPEED                   SetClockSpeed;
  QSPI_CONTROLLER_GET_NUM_CHIP_SELECTS              GetNumChipSelects;
  QSPI_CONTROLLER_APPLY_DEVICE_SPECIFIC_SETTINGS    ApplyDeviceSpecificSettings;
};

extern EFI_GUID  gNVIDIAQspiControllerProtocolGuid;

#endif
