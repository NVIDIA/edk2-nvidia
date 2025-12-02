/** @file

  Bpmp I2c Controller Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BPMP_I2C_PRIVATE_H__
#define __BPMP_I2C_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>
#include <Protocol/I2cBusConfigurationManagement.h>
#include <Protocol/BpmpIpc.h>
#include <Pi/PiI2c.h>

#define BPMP_I2C_ADDL_SLAVES  1
#define BPMP_I2C_SLAVE_AND    0
#define BPMP_I2C_SLAVE_OR     1

#define BPMP_I2C_CMD_TRANSFER  1

#define BPMP_I2C_MAX_SIZE  (120 - 12)
#define BPMP_I2C_READ      0x0001;
#define BPMP_I2C_STOP      0x8000;

#define BPMP_I2C_HEADER_LENGTH       6
#define BPMP_I2C_FULL_HEADER_LENGTH  18

// VRS PSEQ register definitions
#define VRS_CTL_2         0x29
#define VRS_CTL_2_EN_PEC  BIT0

typedef struct {
  UINT16    SlaveAddress;
  UINT16    Flags;
  UINT16    Length;
  UINT8     Data[];
} BPMP_I2C_REQUEST_OP;

typedef struct {
  UINT32    Command;
  UINT32    BusId;
  UINT32    DataSize;
  UINT8     Data[BPMP_I2C_MAX_SIZE];
} BPMP_I2C_REQUEST;

typedef struct {
  UINT32    DataSize;
  UINT32    Data[BPMP_I2C_MAX_SIZE];
} BPMP_I2C_RESPONSE;

#define BPMP_I2C_SIGNATURE  SIGNATURE_32('B','I','2','C')

typedef struct {
  //
  // Standard signature used to identify BpmpI2c private data
  //
  UINT32                                           Signature;

  //
  // Protocol instances produced by this driver
  //
  EFI_I2C_MASTER_PROTOCOL                          I2cMaster;
  EFI_I2C_CONTROLLER_CAPABILITIES                  I2cControllerCapabilities;
  EFI_I2C_ENUMERATE_PROTOCOL                       I2cEnumerate;
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL    I2CConfiguration;
  EFI_DEVICE_PATH                                  *ChildDevicePath;

  //
  // Indicates if the protocols are installed
  //
  BOOLEAN                                          ProtocolsInstalled;

  //
  // Handles
  //
  EFI_HANDLE                                       Parent;
  EFI_HANDLE                                       Child;
  EFI_HANDLE                                       DriverBindingHandle;

  //
  // Devices found in device tree
  //
  EFI_I2C_DEVICE                                   *I2cDevices;
  UINT32                                           *SlaveAddressArray;
  UINTN                                            NumberOfI2cDevices;

  //
  // Event for protocol notification
  //
  NVIDIA_BPMP_IPC_PROTOCOL                         *BpmpIpc;

  UINT32                                           BusId;
  VOID                                             *DeviceTreeBase;
  INT32                                            DeviceTreeNodeOffset;
  UINT32                                           BpmpPhandle;

  // Current transaction info
  UINTN                                            SlaveAddress;
  EFI_I2C_REQUEST_PACKET                           *RequestPacket;
  EFI_EVENT                                        TransactionEvent;
  EFI_STATUS                                       *TransactionStatus;
  EFI_STATUS                                       PrivateTransactionStatus; // Used for async transactions with NULL I2cStatus
  BPMP_I2C_REQUEST                                 Request;
  BPMP_I2C_RESPONSE                                Response;
  INT32                                            MessageError;

  // Transaction processing
  NVIDIA_BPMP_IPC_TOKEN                            BpmpIpcToken;
  BOOLEAN                                          TransferInProgress;
} NVIDIA_BPMP_I2C_PRIVATE_DATA;
#define BPMP_I2C_PRIVATE_DATA_FROM_MASTER(a)     CR(a, NVIDIA_BPMP_I2C_PRIVATE_DATA, I2cMaster, BPMP_I2C_SIGNATURE)
#define BPMP_I2C_PRIVATE_DATA_FROM_ENUMERATE(a)  CR(a, NVIDIA_BPMP_I2C_PRIVATE_DATA, I2cEnumerate, BPMP_I2C_SIGNATURE)

/**
 * Function pointer type for device initialization callbacks
 *
 * @param Private      Pointer to I2C private data
 * @param DeviceIndex  Index of the device in I2cDevices array
 * @param Node         Device tree node offset
 *
 * @return EFI_SUCCESS - Initialization successful
 * @return others      - Failed to initialize
 */
typedef EFI_STATUS (EFIAPI *BPMP_I2C_DEVICE_INIT_FUNC)(
  IN NVIDIA_BPMP_I2C_PRIVATE_DATA  *Private,
  IN UINTN                         DeviceIndex,
  IN INT32                         Node
  );

typedef struct {
  CONST CHAR8                  *Compatibility;
  CONST EFI_GUID               *DeviceType;
  UINTN                        AdditionalSlaves;
  UINTN                        SlaveMasks[BPMP_I2C_ADDL_SLAVES][2];
  BPMP_I2C_DEVICE_INIT_FUNC    InitFunction; ///< Optional initialization function, NULL if not needed
} BPMP_I2C_DEVICE_TYPE_MAP;

///
/// I2C request packet helper structures
///
/// These structures provide convenient wrappers for common I2C transaction patterns
/// that can be cast to EFI_I2C_REQUEST_PACKET for use with EFI_I2C_MASTER_PROTOCOL.
///

/// I2C register write packet
///
/// Used for writing to I2C device registers. Contains a single operation that
/// writes both the register address and data in one transaction.
/// Typical usage: Operation[0] contains register address followed by data bytes.
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN                OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION    Operation[1];
} I2C_REGISTER_WRITE_PACKET;

/// I2C register read packet
///
/// Used for reading from I2C device registers. Contains two operations:
/// Operation[0] writes the register address, Operation[1] reads the data.
/// Typical usage: Write register address, then read register value.
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN                OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION    Operation[2];
} I2C_REGISTER_READ_PACKET;

#endif
