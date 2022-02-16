/** @file

  Bpmp I2c Controller Driver private structures

  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BPMP_I2C_PRIVATE_H__
#define __BPMP_I2C_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>
#include <Protocol/I2cBusConfigurationManagement.h>
#include <Protocol/BpmpIpc.h>

#define BPMP_I2C_ADDL_SLAVES 1
#define BPMP_I2C_SLAVE_AND   0
#define BPMP_I2C_SLAVE_OR    1

typedef struct {
  CONST CHAR8    *Compatibility;
  CONST EFI_GUID *DeviceType;
  UINTN          AdditionalSlaves;
  UINTN          SlaveMasks[BPMP_I2C_ADDL_SLAVES][2];

} BPMP_I2C_DEVICE_TYPE_MAP;

#define BPMP_I2C_CMD_TRANSFER 1

#define BPMP_I2C_MAX_SIZE (120 - 12)
#define BPMP_I2C_READ  0x0001;
#define BPMP_I2C_STOP  0x8000;

#define BPMP_I2C_HEADER_LENGTH      6
#define BPMP_I2C_FULL_HEADER_LENGTH 18

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
  UINT32 DataSize;
  UINT32 Data[BPMP_I2C_MAX_SIZE];
} BPMP_I2C_RESPONSE;

#define BPMP_I2C_SIGNATURE SIGNATURE_32('B','I','2','C')

typedef struct {
  //
  // Standard signature used to identify BpmpI2c private data
  //
  UINT32                                        Signature;

  //
  // Protocol instances produced by this driver
  //
  EFI_I2C_MASTER_PROTOCOL                       I2cMaster;
  EFI_I2C_CONTROLLER_CAPABILITIES               I2cControllerCapabilities;
  EFI_I2C_ENUMERATE_PROTOCOL                    I2cEnumerate;
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL I2CConfiguration;
  EFI_DEVICE_PATH                               *ChildDevicePath;

  //
  // Indicates if the protocols are installed
  //
  BOOLEAN                                       ProtocolsInstalled;

  //
  // Handles
  //
  EFI_HANDLE                                    Parent;
  EFI_HANDLE                                    Child;
  EFI_HANDLE                                    DriverBindingHandle;

  //
  // Devices found in device tree
  //
  EFI_I2C_DEVICE                                *I2cDevices;
  UINT32                                        *SlaveAddressArray;
  UINTN                                         NumberOfI2cDevices;

  //
  // Event for protocol notification
  //
  NVIDIA_BPMP_IPC_PROTOCOL                      *BpmpIpc;

  UINT32                                        BusId;
  VOID                                          *DeviceTreeBase;
  INT32                                         DeviceTreeNodeOffset;

  //Current transaction info
  UINTN                                         SlaveAddress;
  EFI_I2C_REQUEST_PACKET                        *RequestPacket;
  EFI_EVENT                                     TransactionEvent;
  EFI_STATUS                                    *TransactionStatus;
  BPMP_I2C_REQUEST                              Request;
  BPMP_I2C_RESPONSE                             Response;
  INT32                                         MessageError;


  //Transaction processing
  NVIDIA_BPMP_IPC_TOKEN                         BpmpIpcToken;
  BOOLEAN                                       TransferInProgress;

} NVIDIA_BPMP_I2C_PRIVATE_DATA;
#define BPMP_I2C_PRIVATE_DATA_FROM_MASTER(a) CR(a, NVIDIA_BPMP_I2C_PRIVATE_DATA, I2cMaster, BPMP_I2C_SIGNATURE)
#define BPMP_I2C_PRIVATE_DATA_FROM_ENUMERATE(a) CR(a, NVIDIA_BPMP_I2C_PRIVATE_DATA, I2cEnumerate, BPMP_I2C_SIGNATURE)

#endif
