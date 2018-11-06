/** @file

  Regulator Driver private structures

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __REGULATOR_DXE_PRIVATE_H__
#define __REGULATOR_DXE_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/Regulator.h>
#include <Protocol/I2cIo.h>
#include <Protocol/EmbeddedGpio.h>

#define REGULATOR_NOFITY_LIST_SIGNATURE SIGNATURE_32('R','E','G','N')
typedef struct {
  UINT32                     Signature;
  LIST_ENTRY                 Link;
  EFI_EVENT                  Event;
} REGULATOR_NOTIFY_LIST_ENTRY;
#define REGULATOR_NOTIFY_LIST_FROM_LINK(a) CR(a, REGULATOR_NOTIFY_LIST_ENTRY, Link, REGULATOR_NOFITY_LIST_SIGNATURE)

typedef struct {
  CONST CHAR8 *Name;
  UINT8       VoltageRegister;
  UINT8       VoltageMask;
  UINT8       VoltageShift;
  UINTN       MinMicrovolts;
  UINTN       MaxMicrovolts;
  UINTN       MicrovoltStep;
  UINT8       MinVoltSetting;
  UINT8       ConfigRegister;
  UINT8       ConfigMask;
  UINT8       ConfigShift;
  UINT8       ConfigSetting;
} PMIC_REGULATOR_SETTING;

#define REGULATOR_LIST_SIGNATURE SIGNATURE_32('R','E','G','L')
typedef struct {
  UINT32                     Signature;
  LIST_ENTRY                 Link;
  UINT32                     RegulatorId;
  UINTN                      Gpio;
  BOOLEAN                    AlwaysEnabled;
  BOOLEAN                    IsAvailable;
  UINTN                      MinMicrovolts;
  UINTN                      MaxMicrovolts;
  UINTN                      MicrovoltStep;
  CONST CHAR8                *Name;
  PMIC_REGULATOR_SETTING     *PmicSetting;
  LIST_ENTRY                 NotifyList;
} REGULATOR_LIST_ENTRY;
#define REGULATOR_LIST_FROM_LINK(a) CR(a, REGULATOR_LIST_ENTRY, Link, REGULATOR_LIST_SIGNATURE)



#define REGULATOR_SIGNATURE SIGNATURE_32('R','E','G','D')
typedef struct {
  //
  // Standard signature used to identify regulator private data
  //
  UINT32                     Signature;

  NVIDIA_REGULATOR_PROTOCOL  RegulatorProtocol;

  EFI_HANDLE                 ImageHandle;

  VOID                       *DeviceTreeBase;
  UINTN                      DeviceTreeSize;

  LIST_ENTRY                 RegulatorList;
  UINTN                      Regulators;

  EFI_GUID                   *I2cDeviceGuid;
  VOID                       *GpioSearchToken;
  EMBEDDED_GPIO              *GpioProtocol;
  VOID                       *I2cIoSearchToken;
  EFI_I2C_IO_PROTOCOL        *I2cIoProtocol;
} REGULATOR_DXE_PRIVATE;
#define REGULATOR_PRIVATE_DATA_FROM_THIS(a) CR(a, REGULATOR_DXE_PRIVATE, RegulatorProtocol, REGULATOR_SIGNATURE)

///
/// I2C device request
///
/// The EFI_I2C_REQUEST_PACKET describes a single I2C transaction.  The
/// transaction starts with a start bit followed by the first operation
/// in the operation array.  Subsequent operations are separated with
/// repeated start bits and the last operation is followed by a stop bit
/// which concludes the transaction.  Each operation is described by one
/// of the elements in the Operation array.
///
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION Operation [2];
} REGULATOR_I2C_REQUEST_PACKET_2_OPS;
#endif
