/** @file

  SD MMC Controller Driver

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

#include "RegulatorDxePrivate.h"

PMIC_REGULATOR_SETTING Maxim77620Regulators[] = {
    { "sd0" , 0x16, 0xFF, 0x00, 625000, 1387500, 12500, 0x2, 0x1D, 0x30, 0x4, 0x3 },
    { "sd1" , 0x17, 0xFF, 0x00, 625000, 1550000, 12500, 0x2, 0x1E, 0x30, 0x4, 0x3 },
    { "sd2" , 0x18, 0xFF, 0x00, 625000, 3787500, 12500, 0x2, 0x1F, 0x30, 0x4, 0x3 },
    { "sd3" , 0x19, 0xFF, 0x00, 625000, 3787500, 12500, 0x2, 0x20, 0x30, 0x4, 0x3 },
    { "ldo0", 0x23, 0x3F, 0x00, 800000, 2375000, 25000, 0x0, 0x23, 0xC0, 0x6, 0x3 },
    { "ldo1", 0x25, 0x3F, 0x00, 800000, 2375000, 25000, 0x0, 0x25, 0xC0, 0x6, 0x3 },
    { "ldo2", 0x27, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x27, 0xC0, 0x6, 0x3 },
    { "ldo3", 0x29, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x29, 0xC0, 0x6, 0x3 },
    { "ldo4", 0x2B, 0x3F, 0x00, 800000, 1587500, 12500, 0x0, 0x2B, 0xC0, 0x6, 0x3 },
    { "ldo5", 0x2D, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x2D, 0xC0, 0x6, 0x3 },
    { "ldo6", 0x2F, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x2F, 0xC0, 0x6, 0x3 },
    { "ldo7", 0x31, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x31, 0xC0, 0x6, 0x3 },
    { "ldo8", 0x33, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x33, 0xC0, 0x6, 0x3 },
};

PMIC_REGULATOR_SETTING Maxim20024Regulators[] = {
    { "sd0" , 0x16, 0xFF, 0x00, 800000, 1587500, 12500, 0x0, 0x1D, 0x30, 0x4, 0x3 },
    { "sd1" , 0x17, 0xFF, 0x00, 600000, 3787500, 12500, 0x0, 0x1E, 0x30, 0x4, 0x3 },
    { "sd2" , 0x18, 0xFF, 0x00, 600000, 3787500, 12500, 0x0, 0x1F, 0x30, 0x4, 0x3 },
    { "sd3" , 0x19, 0xFF, 0x00, 600000, 3787500, 12500, 0x0, 0x20, 0x30, 0x4, 0x3 },
    { "sd4" , 0x1A, 0xFF, 0x00, 600000, 3787500, 12500, 0x0, 0x21, 0x30, 0x4, 0x3 },
    { "ldo0", 0x23, 0x3F, 0x00, 800000, 2375000, 25000, 0x0, 0x23, 0xC0, 0x6, 0x3 },
    { "ldo1", 0x25, 0x3F, 0x00, 800000, 2375000, 25000, 0x0, 0x25, 0xC0, 0x6, 0x3 },
    { "ldo2", 0x27, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x27, 0xC0, 0x6, 0x3 },
    { "ldo3", 0x29, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x29, 0xC0, 0x6, 0x3 },
    { "ldo4", 0x2B, 0x3F, 0x00, 800000, 1587500, 12500, 0x0, 0x2B, 0xC0, 0x6, 0x3 },
    { "ldo5", 0x2D, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x2D, 0xC0, 0x6, 0x3 },
    { "ldo6", 0x2F, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x2F, 0xC0, 0x6, 0x3 },
    { "ldo7", 0x31, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x31, 0xC0, 0x6, 0x3 },
    { "ldo8", 0x33, 0x3F, 0x00, 800000, 3950000, 50000, 0x0, 0x33, 0xC0, 0x6, 0x3 },
};

/**
 * Notifies all registered listeners on the entry
 * @param Entry - Entry to notify
 */
STATIC
VOID
NotifyEntry (
  IN REGULATOR_LIST_ENTRY *Entry
  )
{
  LIST_ENTRY *NotifyListNode;
  NotifyListNode = GetFirstNode (&Entry->NotifyList);
  while (NotifyListNode != &Entry->NotifyList) {
    REGULATOR_NOTIFY_LIST_ENTRY *NotifyEntry = REGULATOR_NOTIFY_LIST_FROM_LINK (NotifyListNode);
    if (NotifyEntry != NULL) {
      gBS->SignalEvent (NotifyEntry->Event);
    }
    NotifyListNode = GetNextNode (&Entry->NotifyList, NotifyListNode);
  }
}

/**
 * Finds the regulator entry for the specified name or id.
 *
 * @param RegulatorList   - List of all regulators
 * @param RegulatorId     - Id to match
 * @param RegulatorName   - Name to match, this is used if not NULL
 *
 * @return Pointer to list entry
 * @return NULL, if not found
 */
STATIC
REGULATOR_LIST_ENTRY *
FindRegulatorEntry (
    IN LIST_ENTRY  *RegulatorList,
    IN UINT32      RegulatorId,
    IN CONST CHAR8 *RegulatorName OPTIONAL
    )
{
  LIST_ENTRY *ListNode;
  if (NULL == RegulatorList) {
    return NULL;
  }

  ListNode = GetFirstNode (RegulatorList);
  while (ListNode != RegulatorList) {
    REGULATOR_LIST_ENTRY *Entry = REGULATOR_LIST_FROM_LINK (ListNode);
    if (Entry != NULL) {
      if (RegulatorName != NULL) {
        if ((Entry->Name != NULL) &&
            (0 == AsciiStrCmp (RegulatorName, Entry->Name))) {
          return Entry;
        }
      } else if (Entry->RegulatorId == RegulatorId) {
        return Entry;
      }
    }
    ListNode = GetNextNode (RegulatorList, ListNode);
  }
  return NULL;
}

/**
 * Reads byte from PMIC address
 * @param I2cIoProtocol - I2cIo protocol for Pmic
 * @param Address       - Address to read
 * @param Value         - Pointer to data to read to.
 * @return EFI_SUCCESS - Data read
 * @return others      - Error in read
 */
STATIC
EFI_STATUS
ReadPmicRegister (
  IN  EFI_I2C_IO_PROTOCOL *I2cIoProtocol,
  IN  UINT8               Address,
  OUT UINT8               *Value
  )
{
  EFI_STATUS Status;
  REGULATOR_I2C_REQUEST_PACKET_2_OPS Operation;
  if ((NULL == I2cIoProtocol) ||
      (NULL == Value)) {
    return EFI_INVALID_PARAMETER;
  }

  Operation.OperationCount = 2;
  Operation.Operation[0].Flags = 0;
  Operation.Operation[0].LengthInBytes = 1;
  Operation.Operation[0].Buffer = &Address;
  Operation.Operation[1].Flags = I2C_FLAG_READ;
  Operation.Operation[1].LengthInBytes = 1;
  Operation.Operation[1].Buffer = Value;
  Status = I2cIoProtocol->QueueRequest (
                          I2cIoProtocol,
                          0,
                          NULL,
                          (EFI_I2C_REQUEST_PACKET *)&Operation,
                          NULL
                          );
  DEBUG ((EFI_D_VERBOSE, "%a: 0x%02x <- 0x%02x, %r\r\n", __FUNCTION__, *Value, Address, Status));
  return Status;
}

/**
 * Writes byte to PMIC address
 * @param I2cIoProtocol - I2cIo protocol for Pmic
 * @param Address       - Address to write to
 * @param Value         - Data to write to.
 * @return EFI_SUCCESS - Data read
 * @return others      - Error in read
 */
STATIC
EFI_STATUS
WritePmicRegister (
  IN  EFI_I2C_IO_PROTOCOL *I2cIoProtocol,
  IN  UINT8               Address,
  OUT UINT8               Value
  )
{
  EFI_STATUS Status;
  EFI_I2C_REQUEST_PACKET Operation;
  UINT8                  Data[2];
  if (NULL == I2cIoProtocol) {
    return EFI_INVALID_PARAMETER;
  }

  Data[0] = Address;
  Data[1] = Value;
  Operation.OperationCount = 1;
  Operation.Operation[0].Flags = 0;
  Operation.Operation[0].LengthInBytes = 2;
  Operation.Operation[0].Buffer = Data;
  Status = I2cIoProtocol->QueueRequest (
                          I2cIoProtocol,
                          0,
                          NULL,
                          &Operation,
                          NULL
                          );
  DEBUG ((EFI_D_VERBOSE, "%a: 0x%02x -> 0x%02x, %r\r\n", __FUNCTION__, Value, Address, Status));
  return Status;
}

/**
  This function gets information about the specified regulator.

  @param[in]     This                The instance of the NVIDIA_REGULATOR_PROTOCOL.
  @param[in]     RegulatorId         Id of the regulator.
  @param[out]    RegulatorInfo       Pointer that will contain the regulator info

  @return EFI_SUCCESS                Regulator info returned.
  @return EFI_NOT_FOUND              Regulator is not supported on target.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
STATIC
EFI_STATUS
RegulatorGetInfo (
  IN  NVIDIA_REGULATOR_PROTOCOL  *This,
  IN  UINT32                     RegulatorId,
  OUT REGULATOR_INFO             *RegulatorInfo
  )
{
  REGULATOR_DXE_PRIVATE *Private;
  REGULATOR_LIST_ENTRY  *Entry;
  EFI_STATUS            Status;
  if ((This == NULL) ||
      (RegulatorInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = FindRegulatorEntry (&Private->RegulatorList, RegulatorId, NULL);
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }

  RegulatorInfo->AlwaysEnabled = Entry->AlwaysEnabled;
  RegulatorInfo->IsAvailable   = Entry->IsAvailable;
  RegulatorInfo->MinMicrovolts = Entry->MinMicrovolts;
  RegulatorInfo->MaxMicrovolts = Entry->MaxMicrovolts;
  RegulatorInfo->MicrovoltStep = Entry->MicrovoltStep;
  RegulatorInfo->Name          = Entry->Name;

  if (RegulatorInfo->IsAvailable) {
    if ((Entry->Gpio != 0) && !Entry->AlwaysEnabled) {
      EMBEDDED_GPIO_MODE GpioMode;
      RegulatorInfo->CurrentMicrovolts = Entry->MinMicrovolts;
      Status = Private->GpioProtocol->GetMode (
                 Private->GpioProtocol,
                 Entry->Gpio,
                 &GpioMode);
      if (EFI_ERROR (Status)) {
        RegulatorInfo->IsAvailable   = FALSE;
        Entry->IsAvailable           = FALSE;
        RegulatorInfo->IsEnabled     = FALSE;
      } else {
        RegulatorInfo->IsEnabled = (GpioMode == GPIO_MODE_OUTPUT_1);
      }
    } else if ((Entry->PmicSetting != NULL) && !Entry->AlwaysEnabled) {
      UINT8 Data;
      Status = ReadPmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->ConfigRegister, &Data);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to read configuration register: %r\r\n", __FUNCTION__, Status));
        return Status;
      }
      if (((Data & Entry->PmicSetting->ConfigMask) >> Entry->PmicSetting->ConfigShift) == Entry->PmicSetting->ConfigSetting) {
        RegulatorInfo->IsEnabled = TRUE;
      } else {
        RegulatorInfo->IsEnabled = FALSE;
      }

      Status = ReadPmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->VoltageRegister, &Data);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to read voltage register: %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      Data = ((Data & Entry->PmicSetting->VoltageMask) >> Entry->PmicSetting->VoltageShift) - Entry->PmicSetting->MinVoltSetting;
      RegulatorInfo->CurrentMicrovolts = Data * Entry->MicrovoltStep + Entry->PmicSetting->MinMicrovolts;
    } else {
      RegulatorInfo->CurrentMicrovolts = Entry->MinMicrovolts;
      RegulatorInfo->IsEnabled = Entry->AlwaysEnabled;
    }
  } else {
    RegulatorInfo->CurrentMicrovolts = 0;
    RegulatorInfo->IsEnabled = FALSE;
  }
  return EFI_SUCCESS;
}

/**
  This function gets the regulator id from the name specified

  @param[in]     This                The instance of the NVIDIA_REGULATOR_PROTOCOL.
  @param[in]     Name                Name of the regulator
  @param[out]    RegulatorId         Pointer to the id of the regulator.

  @return EFI_SUCCESS                Regulator id returned.
  @return EFI_NOT_FOUND         Pointer to the i     Regulator is not supported on target.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
STATIC
EFI_STATUS
RegulatorGetIdFromName (
  IN  NVIDIA_REGULATOR_PROTOCOL  *This,
  IN  CONST CHAR8                *Name,
  OUT UINT32                     *RegulatorId
  )
{
  REGULATOR_DXE_PRIVATE *Private;
  REGULATOR_LIST_ENTRY  *Entry;
  if ((This == NULL) ||
      (Name == NULL) ||
      (RegulatorId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = FindRegulatorEntry (&Private->RegulatorList, 0, Name);
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }
  *RegulatorId = Entry->RegulatorId;
  return EFI_SUCCESS;
}

/**
 * This function retrieves the ids for all the regulators on the system
 *
  @param[in]      This               The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in,out] BufferSize         On input size of Regulator Id buffer in bytes, on output size required.
 * @param[out]    RegulatorIds       Pointer to array of regulator ids to fill out
 *
 * @return EFI_SUCCESS               Regulators returned
 * @return EFI_INVALID_PARAMETER     BufferSize is not 0 but RegulatorIds is NULL
 * @return EFI_BUFFER_TOO_SMALL      BufferSize is to small on input for supported regulators.
 */
STATIC
EFI_STATUS
RegulatorGetRegulators (
  IN     NVIDIA_REGULATOR_PROTOCOL  *This,
  IN OUT UINTN                      *BufferSize,
  OUT    UINT32                     *RegulatorIds OPTIONAL
  )
{
  REGULATOR_DXE_PRIVATE *Private;
  UINTN                 NeededSize;
  LIST_ENTRY            *ListNode;
  UINTN                 CurrentRegulator = 0;

  if ((This == NULL) ||
      (BufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  NeededSize = Private->Regulators * sizeof (UINT32);
  if (*BufferSize < NeededSize) {
    *BufferSize = NeededSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  if (NULL == RegulatorIds) {
    return EFI_INVALID_PARAMETER;
  }

  ListNode = GetFirstNode (&Private->RegulatorList);
  while (ListNode != &Private->RegulatorList) {
    REGULATOR_LIST_ENTRY *Entry = REGULATOR_LIST_FROM_LINK (ListNode);
    if (Entry != NULL) {
      if (CurrentRegulator >= Private->Regulators) {
        return EFI_DEVICE_ERROR;
      }
      RegulatorIds[CurrentRegulator] = Entry->RegulatorId;
      CurrentRegulator++;
    }
    ListNode = GetNextNode (&Private->RegulatorList, ListNode);
  }
  ASSERT (CurrentRegulator == Private->Regulators);
  return EFI_SUCCESS;
}

/**
 * This function notifies the caller of that the state of the regulator has changed.
 * This can be for regulator being made available or change in enable status or voltage.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to be notified on.
 * @param[in] Event                  Event that will be signaled when regulator state changes.
 *
 * @return EFI_SUCCESS               Registration was successful.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_DEVICE_ERROR          Notification registration failed.
 */
STATIC
EFI_STATUS
RegulatorNotifyStateChange (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN EFI_EVENT                  Event
  )
{
  REGULATOR_DXE_PRIVATE       *Private;
  REGULATOR_LIST_ENTRY        *Entry;
  REGULATOR_NOTIFY_LIST_ENTRY *NotifyEntry;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = FindRegulatorEntry (&Private->RegulatorList, RegulatorId, NULL);
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }

  NotifyEntry = (REGULATOR_NOTIFY_LIST_ENTRY *)AllocatePool (sizeof (REGULATOR_NOTIFY_LIST_ENTRY));
  if (NULL == NotifyEntry) {
    return EFI_OUT_OF_RESOURCES;
  }

  NotifyEntry->Signature = REGULATOR_NOFITY_LIST_SIGNATURE;
  NotifyEntry->Event = Event;
  InsertTailList (&Entry->NotifyList, &NotifyEntry->Link);
  return EFI_SUCCESS;
}

/**
 * This function enables or disables the specified regulator.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to be enable/disable.
 * @param[in] Enable                 TRUE to enable, FALSE to disable.
 *
 * @return EFI_SUCCESS               Regulator state change occurred.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_UNSUPPORTED           Regulator does not support being enabled or disabled
 * @return EFI_DEVICE_ERROR          Other error occurred.
 */
STATIC
EFI_STATUS
RegulatorEnable (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN BOOLEAN                    Enable
  )
{
  REGULATOR_DXE_PRIVATE *Private;
  REGULATOR_LIST_ENTRY  *Entry;
  EFI_STATUS            Status;
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = FindRegulatorEntry (&Private->RegulatorList, RegulatorId, NULL);
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }

  if (!Entry->IsAvailable) {
    return EFI_NOT_READY;
  }

  if (Entry->AlwaysEnabled) {
    if (Enable) {
      return EFI_SUCCESS;
    } else {
      return EFI_DEVICE_ERROR;
    }
  }

  if (Entry->Gpio != 0) {
    EMBEDDED_GPIO_MODE GpioMode;
    if (Enable) {
      GpioMode = GPIO_MODE_OUTPUT_1;
    } else {
      GpioMode = GPIO_MODE_OUTPUT_0;
    }
    Status = Private->GpioProtocol->Set (
               Private->GpioProtocol,
               Entry->Gpio,
               GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to set gpio 0x%x mode: %r\r\n", __FUNCTION__, Entry->Gpio, Status));
      return EFI_DEVICE_ERROR;
    }
    NotifyEntry (Entry);
    return EFI_SUCCESS;;
  } else if (Entry->PmicSetting != NULL) {
    UINT8 DataOriginal;
    UINT8 DataNew;
    Status = ReadPmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->ConfigRegister, &DataOriginal);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to read configuration register: %r\r\n", __FUNCTION__, Status));
      return Status;
    }
    DataNew = DataOriginal;
    DataNew &= ~Entry->PmicSetting->ConfigMask;
    if (Enable) {
      DataNew |= (Entry->PmicSetting->ConfigSetting << Entry->PmicSetting->ConfigShift);
    }
    if (DataNew != DataOriginal) {
      Status = WritePmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->ConfigRegister, DataNew);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to write configuration register: %r\r\n", __FUNCTION__, Status));
        return Status;
      }
      NotifyEntry (Entry);
    }
    return Status;
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
 * This function sets the voltage the specified regulator.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to set voltage on.
 * @param[in] Microvolts             Voltage in microvolts.
 *
 * @return EFI_SUCCESS               Regulator state change occurred.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_UNSUPPORTED           Regulator does not support voltage change.
 * @return EFI_INVALID_PARAMETER     Voltage specified is not supported.
 * @return EFI_DEVICE_ERROR          Other error occurred.
 */
STATIC
EFI_STATUS
RegulatorSetVoltage (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN UINTN                      Microvolts
  )
{
  REGULATOR_DXE_PRIVATE *Private;
  REGULATOR_LIST_ENTRY  *Entry;
  EFI_STATUS            Status;
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Private = REGULATOR_PRIVATE_DATA_FROM_THIS (This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = FindRegulatorEntry (&Private->RegulatorList, RegulatorId, NULL);
  if (Entry == NULL) {
    return EFI_NOT_FOUND;
  }

  if (!Entry->IsAvailable) {
    return EFI_NOT_READY;
  }

  if ((Microvolts < Entry->MinMicrovolts) ||
      (Microvolts > Entry->MaxMicrovolts)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Entry->PmicSetting != NULL) &&
      (!Entry->AlwaysEnabled)) {
    UINT8 DataOriginal;
    UINT8 DataNew;
    Status = ReadPmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->VoltageRegister, &DataOriginal);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to read voltage register: %r\r\n", __FUNCTION__, Status));
      return Status;
    }
    Microvolts -= Entry->PmicSetting->MinMicrovolts;
    Microvolts /= Entry->PmicSetting->MicrovoltStep;
    Microvolts += Entry->PmicSetting->MinVoltSetting;
    DataNew = DataOriginal & ~Entry->PmicSetting->VoltageMask;
    DataNew |= (Microvolts << Entry->PmicSetting->VoltageShift);
    if (DataNew != DataOriginal) {
      Status = WritePmicRegister (Private->I2cIoProtocol, Entry->PmicSetting->VoltageRegister, DataNew);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to write voltage register: %r\r\n", __FUNCTION__, Status));
        return Status;
      }
      NotifyEntry (Entry);
    }
    return Status;
  } else {
    //Fixed regulator
    return EFI_SUCCESS;
  }
}

/**
 * Notification when i2cio protocol is installed
 * @param Event   - Event that is notified
 * @param Context - Context that was present when registed.
 */
STATIC
VOID
I2cIoProtocolReady (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS            Status = EFI_SUCCESS;
  REGULATOR_DXE_PRIVATE *Private = (REGULATOR_DXE_PRIVATE *)Context;
  LIST_ENTRY            *ListNode;

  if (Private == NULL) {
    return;
  }

  while (!EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (
                    &gEfiI2cIoProtocolGuid,
                    Private->I2cIoSearchToken,
                    (VOID **)&Private->I2cIoProtocol);
    if (EFI_ERROR (Status)) {
      return;
    }

    if (CompareGuid (Private->I2cIoProtocol->DeviceGuid, Private->I2cDeviceGuid)) {
      break;
    }
  }
  gBS->CloseEvent (Event);


  DEBUG ((EFI_D_VERBOSE, "%a: Ready!!!\r\n", __FUNCTION__));
  ListNode = GetFirstNode (&Private->RegulatorList);
  while (ListNode != &Private->RegulatorList) {
    REGULATOR_LIST_ENTRY *Entry;
    Entry = REGULATOR_LIST_FROM_LINK (ListNode);
    if (NULL != Entry) {
      if (Entry->PmicSetting != NULL) {
        Entry->IsAvailable = TRUE;
        NotifyEntry (Entry);
      }
    }
    ListNode = GetNextNode (&Private->RegulatorList, ListNode);
  }

  gBS->InstallMultipleProtocolInterfaces (
         &Private->ImageHandle,
         &gNVIDIAPmicRegulatorsPresentProtocolGuid,
         NULL,
         NULL
         );
  if (NULL != Private->I2cIoProtocol) {
    gBS->InstallMultipleProtocolInterfaces (
           &Private->ImageHandle,
           &gNVIDIAAllRegulatorsPresentProtocolGuid,
           NULL,
           NULL
           );
  }
}

/**
 * Notification when gpio protocol is installed
 * @param Event   - Event that is notified
 * @param Context - Context that was present when registed.
 */
STATIC
VOID
GpioProtocolReady (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS            Status;
  REGULATOR_DXE_PRIVATE *Private = (REGULATOR_DXE_PRIVATE *)Context;
  LIST_ENTRY            *ListNode;

  if (Private == NULL) {
    return;
  }

  Status = gBS->LocateProtocol (
                  &gEmbeddedGpioProtocolGuid,
                  Private->GpioSearchToken,
                  (VOID **)&Private->GpioProtocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  gBS->CloseEvent (Event);

  DEBUG ((EFI_D_VERBOSE, "%a: Ready!!!\r\n",  __FUNCTION__));
  ListNode = GetFirstNode (&Private->RegulatorList);
  while (ListNode != &Private->RegulatorList) {
    REGULATOR_LIST_ENTRY *Entry;
    Entry = REGULATOR_LIST_FROM_LINK (ListNode);
    if (NULL != Entry) {
      if (Entry->Gpio != 0) {
        Entry->IsAvailable = TRUE;
        NotifyEntry (Entry);
      }
    }
    ListNode = GetNextNode (&Private->RegulatorList, ListNode);
  }

  gBS->InstallMultipleProtocolInterfaces (
         &Private->ImageHandle,
         &gNVIDIAFixedRegulatorsPresentProtocolGuid,
         NULL,
         NULL
         );
  if (NULL != Private->I2cIoProtocol) {
    gBS->InstallMultipleProtocolInterfaces (
           &Private->ImageHandle,
           &gNVIDIAAllRegulatorsPresentProtocolGuid,
           NULL,
           NULL
           );
  }
}

/**
 * Adds all fixed (gpio) regulators in device tree to list
 *
 * @param[in] Private  - Pointer to module private data
 *
 * @return EFI_SUCCESS - Nodes added
 * @return others      - Failed to add nodes
 */
STATIC
EFI_STATUS
AddFixedRegulators (
  IN REGULATOR_DXE_PRIVATE *Private
  )
{
  INT32      NodeOffset = -1;
  BOOLEAN    Pass = 0; //Pass 0: fixed, always on, pass 1: Gpio

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  do {
    REGULATOR_LIST_ENTRY *ListEntry = NULL;
    INT32                PropertySize;
    CONST VOID           *Property = NULL;
    if (Pass == 0) {
      NodeOffset = fdt_node_offset_by_compatible (
                     Private->DeviceTreeBase,
                     NodeOffset,
                     "regulator-fixed"
                     );
    } else {
      NodeOffset = fdt_node_offset_by_compatible (
                     Private->DeviceTreeBase,
                     NodeOffset,
                     "regulator-fixed-sync"
                     );
    }
    if (NodeOffset <= 0) {
      Pass++;
      continue;
    }

    ListEntry = AllocateZeroPool (sizeof (REGULATOR_LIST_ENTRY));
    if (NULL == ListEntry) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    InitializeListHead (&ListEntry->NotifyList);
    ListEntry->Signature = REGULATOR_LIST_SIGNATURE;
    InsertTailList (&Private->RegulatorList, &ListEntry->Link);
    Private->Regulators++;
    ListEntry->RegulatorId = fdt_get_phandle (Private->DeviceTreeBase, NodeOffset);
    Property = fdt_getprop (Private->DeviceTreeBase, NodeOffset, "regulator-always-on", NULL);
    ListEntry->AlwaysEnabled = (Property != NULL);
    Property = fdt_getprop (Private->DeviceTreeBase, NodeOffset, "gpio", &PropertySize);
    if ((NULL != Property) && (PropertySize == (3 * sizeof (UINT32)))) {
      CONST UINT32 *Data = (CONST UINT32 *)Property;
      UINT32 Controller = SwapBytes32 (Data[0]);
      UINT32 Gpio = SwapBytes32 (Data[1]);
      ListEntry->Gpio = GPIO (Controller, Gpio);
      ListEntry->IsAvailable = FALSE;
    } else {
      ListEntry->Gpio = 0;
      ListEntry->IsAvailable = TRUE;
    }
    if (!ListEntry->IsAvailable) {
      ListEntry->IsAvailable = ListEntry->AlwaysEnabled;
    }
    Property = fdt_getprop (Private->DeviceTreeBase, NodeOffset, "regulator-min-microvolt", &PropertySize);
    if ((NULL != Property) && (PropertySize == sizeof (UINT32))) {
      UINT32 Microvolts = SwapBytes32 (*(UINT32 *)Property);
      ListEntry->MinMicrovolts = Microvolts;
      ListEntry->MaxMicrovolts = Microvolts;
    }
    ListEntry->MicrovoltStep = 0;
    ListEntry->Name = (CONST CHAR8 *)fdt_getprop (Private->DeviceTreeBase, NodeOffset, "regulator-name", NULL);
  } while (Pass != 2);
  return EFI_SUCCESS;
}

/**
 * Finds the pmic structure for the entry
 * @param Private        - Driver private date
 * @param ListEntry      - List to update
 * @param SubNodeOffset  - Nod offset
 * @return EFI_SUCCESS   - Node updated
 * @return EFI_NOT_FOUND - Node not found in list
 */
STATIC
EFI_STATUS
LookupPmicInfo (
  IN REGULATOR_DXE_PRIVATE    *Private,
  IN OUT REGULATOR_LIST_ENTRY *ListEntry,
  INT32                       SubNodeOffset
  )
{
  UINTN                  Index;
  UINTN                  ArraySize;
  PMIC_REGULATOR_SETTING *RegulatorMap;

  CONST CHAR8 *Name = fdt_get_name (Private->DeviceTreeBase, SubNodeOffset, NULL);
  if (NULL == Name) {
    return EFI_NOT_FOUND;
  }

  if (CompareGuid (Private->I2cDeviceGuid, &gNVIDIAI2cMaxim77620)) {
    ArraySize = ARRAY_SIZE (Maxim77620Regulators);
    RegulatorMap = Maxim77620Regulators;
  } else {
    ArraySize = ARRAY_SIZE (Maxim20024Regulators);
    RegulatorMap = Maxim20024Regulators;
  }

  for (Index = 0; Index < ArraySize; Index++) {
    if (0 == AsciiStrCmp (Name, RegulatorMap[Index].Name)) {
      ListEntry->PmicSetting = &RegulatorMap[Index];
      ListEntry->MicrovoltStep = ListEntry->PmicSetting->MicrovoltStep;
      if (ListEntry->MinMicrovolts < ListEntry->PmicSetting->MinMicrovolts) {
        ListEntry->MinMicrovolts = ListEntry->PmicSetting->MinMicrovolts;
      } else if (ListEntry->MinMicrovolts > ListEntry->PmicSetting->MaxMicrovolts) {
        ListEntry->MinMicrovolts = ListEntry->PmicSetting->MaxMicrovolts;
      }

      if ((ListEntry->MaxMicrovolts > ListEntry->PmicSetting->MaxMicrovolts) ||
          (ListEntry->MaxMicrovolts == 0)) {
        ListEntry->MaxMicrovolts = ListEntry->PmicSetting->MaxMicrovolts;
      } else if (ListEntry->MaxMicrovolts < ListEntry->PmicSetting->MinMicrovolts) {
        ListEntry->MaxMicrovolts = ListEntry->PmicSetting->MinMicrovolts;
      }
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

/**
 * Adds all pmic regulators in device tree to list
 *
 * @param[in] Private  - Pointer to module private data
 *
 * @return EFI_SUCCESS - Nodes added
 * @return others      - Failed to add nodes
 */
STATIC
EFI_STATUS
AddPmicRegulators (
  IN REGULATOR_DXE_PRIVATE *Private
  )
{
  INT32      NodeOffset = -1;
  INT32      SubNodeOffset;
  EFI_STATUS Status;

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  Private->I2cDeviceGuid = &gNVIDIAI2cMaxim77620;
  NodeOffset = fdt_node_offset_by_compatible (
                 Private->DeviceTreeBase,
                 NodeOffset,
                 "maxim,max77620"
                 );
  if (NodeOffset <= 0) {
    Private->I2cDeviceGuid = &gNVIDIAI2cMaxim20024;
    NodeOffset = fdt_node_offset_by_compatible (
                   Private->DeviceTreeBase,
                   NodeOffset,
                   "maxim,max20024"
                   );
  }
  if (NodeOffset <= 0) {
    DEBUG ((EFI_D_ERROR, "%a, No pmic nodes found.\r\n"));
    return EFI_SUCCESS;
  }

  NodeOffset = fdt_subnode_offset (
                 Private->DeviceTreeBase,
                 NodeOffset,
                 "regulators"
                 );
  if (NodeOffset <= 0) {
    DEBUG ((EFI_D_ERROR, "%a, No pmic regulator nodes found.\r\n"));
    return EFI_SUCCESS;
  }

  fdt_for_each_subnode (SubNodeOffset, Private->DeviceTreeBase, NodeOffset) {
    REGULATOR_LIST_ENTRY *ListEntry = NULL;
    INT32                PropertySize;
    CONST VOID           *Property = NULL;

    ListEntry = AllocateZeroPool (sizeof (REGULATOR_LIST_ENTRY));
    if (NULL == ListEntry) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    InitializeListHead (&ListEntry->NotifyList);
    ListEntry->Signature = REGULATOR_LIST_SIGNATURE;
    InsertTailList (&Private->RegulatorList, &ListEntry->Link);
    Private->Regulators++;
    ListEntry->RegulatorId = fdt_get_phandle (Private->DeviceTreeBase, SubNodeOffset);
    Property = fdt_getprop (Private->DeviceTreeBase, SubNodeOffset, "regulator-always-on", NULL);
    ListEntry->AlwaysEnabled = (Property != NULL);
    ListEntry->IsAvailable = ListEntry->AlwaysEnabled;
    Property = fdt_getprop (Private->DeviceTreeBase, SubNodeOffset, "regulator-min-microvolt", &PropertySize);
    if ((NULL != Property) && (PropertySize == sizeof (UINT32))) {
      UINT32 Microvolts = SwapBytes32 (*(UINT32 *)Property);
      ListEntry->MinMicrovolts = Microvolts;
    }
    Property = fdt_getprop (Private->DeviceTreeBase, SubNodeOffset, "regulator-max-microvolt", &PropertySize);
    if ((NULL != Property) && (PropertySize == sizeof (UINT32))) {
      UINT32 Microvolts = SwapBytes32 (*(UINT32 *)Property);
      ListEntry->MaxMicrovolts = Microvolts;
    }
    ListEntry->MicrovoltStep = 0;
    ListEntry->Name = (CONST CHAR8 *)fdt_getprop (Private->DeviceTreeBase, SubNodeOffset, "regulator-name", NULL);
    Status = LookupPmicInfo (Private, ListEntry, SubNodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to get pmic info: %x, %r\r\n", __FUNCTION__, ListEntry->RegulatorId, Status));
      return Status;
    }
  }
  return EFI_SUCCESS;
}

/**
 * Adds all regulators in device tree to list
 *
 * @param[in] Private  - Pointer to module private data
 *
 * @return EFI_SUCCESS - Nodes added
 * @return others      - Failed to add nodes
 */
STATIC
EFI_STATUS
BuildRegulatorNodes (
  IN REGULATOR_DXE_PRIVATE *Private
  )
{
  EFI_STATUS Status;
  LIST_ENTRY *ListNode;

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DtPlatformLoadDtb (&Private->DeviceTreeBase, &Private->DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a failed to get device tree: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Status = AddFixedRegulators (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a failed to add fixed regulators: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Status = AddPmicRegulators (Private);
  if (EFI_ERROR ((Status))) {
    DEBUG ((EFI_D_ERROR, "%a failed to add pmic regulators: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  ListNode = GetFirstNode (&Private->RegulatorList);
  while (ListNode != &Private->RegulatorList) {
    REGULATOR_LIST_ENTRY *Entry;
    Entry = REGULATOR_LIST_FROM_LINK (ListNode);
    if (NULL != Entry) {
      if (Entry->PmicSetting != NULL) {
        DEBUG ((EFI_D_VERBOSE,
            "%a: Node 0x%04x, Name %a, PMIC Name %a, AlwaysEnabled %u, Available %u, Min %u, Max %u, Step, %u\r\n",
            __FUNCTION__,
            Entry->RegulatorId,
            Entry->Name,
            Entry->PmicSetting->Name,
            Entry->AlwaysEnabled,
            Entry->IsAvailable,
            Entry->MinMicrovolts,
            Entry->MaxMicrovolts,
            Entry->MicrovoltStep
            ));
      } else if (Entry->Gpio != 0) {
        DEBUG ((EFI_D_VERBOSE,
            "%a: Node 0x%04x, Name %a, Gpio 0x%08x, AlwaysEnabled %u, Available %u, Min %u, Max %u, Step, %u\r\n",
            __FUNCTION__,
            Entry->RegulatorId,
            Entry->Name,
            Entry->Gpio,
            Entry->AlwaysEnabled,
            Entry->IsAvailable,
            Entry->MinMicrovolts,
            Entry->MaxMicrovolts,
            Entry->MicrovoltStep
            ));
      } else {
        DEBUG ((EFI_D_VERBOSE,
            "%a: Node 0x%04x, Name %a, AlwaysEnabled %u, Available %u, Min %u, Max %u, Step, %u\r\n",
            __FUNCTION__,
            Entry->RegulatorId,
            Entry->Name,
            Entry->AlwaysEnabled,
            Entry->IsAvailable,
            Entry->MinMicrovolts,
            Entry->MaxMicrovolts,
            Entry->MicrovoltStep
            ));
      }
    }
    ListNode = GetNextNode (&Private->RegulatorList, ListNode);
  }
ErrorExit:
  if (EFI_ERROR (Status)) {
    while (!IsListEmpty (&Private->RegulatorList)) {
      REGULATOR_LIST_ENTRY *Entry;
      LIST_ENTRY           *Node = GetFirstNode (&Private->RegulatorList);
      RemoveEntryList (Node);
      Entry = REGULATOR_LIST_FROM_LINK (Node);
      if (NULL != Entry) {
        FreePool (Entry);
      }
    }
    Private->Regulators = 0;
  }
  return Status;
}

/**
  Initialize the Regulator Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
RegulatorDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS            Status;
  EFI_EVENT             GpioReadyEvent = NULL;
  EFI_EVENT             I2cIoReadyEvent = NULL;
  REGULATOR_DXE_PRIVATE *Private = NULL;
  BOOLEAN               ProtocolInstalled = FALSE;

  Private = AllocatePool (sizeof (REGULATOR_DXE_PRIVATE));
  if (NULL == Private) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate private data stucture: %r\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature = REGULATOR_SIGNATURE;
  Private->RegulatorProtocol.GetInfo = RegulatorGetInfo;
  Private->RegulatorProtocol.GetIdFromName = RegulatorGetIdFromName;
  Private->RegulatorProtocol.GetRegulators = RegulatorGetRegulators;
  Private->RegulatorProtocol.NotifyStateChange = RegulatorNotifyStateChange;
  Private->RegulatorProtocol.Enable = RegulatorEnable;
  Private->RegulatorProtocol.SetVoltage = RegulatorSetVoltage;
  InitializeListHead (&Private->RegulatorList);
  Private->Regulators = 0;
  Private->I2cDeviceGuid = NULL;
  Private->GpioProtocol = NULL;
  Private->I2cIoProtocol = NULL;
  Private->ImageHandle = ImageHandle;

  Status = BuildRegulatorNodes (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to parse regulator data: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  GpioReadyEvent = EfiCreateProtocolNotifyEvent (
                     &gEmbeddedGpioProtocolGuid,
                     TPL_CALLBACK,
                     GpioProtocolReady,
                     Private,
                     &Private->GpioSearchToken
                   );
  if (NULL == GpioReadyEvent) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to create gpio notification event\r\n", __FUNCTION__, Status));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  I2cIoReadyEvent = EfiCreateProtocolNotifyEvent (
                      &gEfiI2cIoProtocolGuid,
                      TPL_CALLBACK,
                      I2cIoProtocolReady,
                      Private,
                      &Private->I2cIoSearchToken
                   );
  if (NULL == I2cIoReadyEvent) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to create I2cIo notification event\r\n", __FUNCTION__, Status));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIARegulatorProtocolGuid,
                  &Private->RegulatorProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to install protocols: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }
  ProtocolInstalled = TRUE;

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (ProtocolInstalled) {
      gBS->UninstallMultipleProtocolInterfaces (
             ImageHandle,
             &gNVIDIARegulatorProtocolGuid,
             &Private->RegulatorProtocol,
             NULL
             );
    }
    if (NULL != I2cIoReadyEvent) {
      gBS->CloseEvent (I2cIoReadyEvent);
    }
    if (NULL != GpioReadyEvent) {
      gBS->CloseEvent (GpioReadyEvent);
    }
    FreePool (Private);
  }
  return Status;
}
