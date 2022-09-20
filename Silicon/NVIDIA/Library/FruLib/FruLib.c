/** @file
  This file defines the various areas in the FRU and their common format.

  Copyright (c) 2003 - 2022 Sun Microsystems, Inc.  All Rights Reserved.
  Copyright (c) 2022 - Nvidia Corporation.  All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <ArmNameSpaceObjects.h>
#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnStorage.h>
#include <Library/PrintLib.h>
#include <Library/FruLib.h>
#include "FruLibPrivate.h"

UINT8            mRecordCount = 0;
FRU_DEVICE_INFO  *mFruRecordInfo[MAX_NUMBER_OF_FRU_DEVICE_IDS];

/**
 * Print the contents of each Fru Record that stores the parsed FRU data.
 *
 * **/
VOID
PrintRecords (
  VOID
  )
{
  UINT8  Index;
  UINT8  Count;

  DEBUG ((DEBUG_VERBOSE, "%a, Number of Fru Records is: %d\n", __FUNCTION__, mRecordCount));
  for (Index = 0; Index < mRecordCount; Index++) {
    DEBUG ((DEBUG_VERBOSE, "Fru Device id: %d\n", mFruRecordInfo[Index]->FruDeviceId));
    DEBUG ((DEBUG_VERBOSE, "Fru Device Description:%a\n", mFruRecordInfo[Index]->FruDeviceDescription));
    DEBUG ((DEBUG_VERBOSE, "Chassis Type: %d \n", mFruRecordInfo[Index]->ChassisType));
    DEBUG ((DEBUG_VERBOSE, "Chassis partnum: %a\n", mFruRecordInfo[Index]->ChassisPartNum));
    DEBUG ((DEBUG_VERBOSE, "Chassis serial: %a\n", mFruRecordInfo[Index]->ChassisSerial));
    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->ChassisExtra[Count]) {
        DEBUG ((DEBUG_VERBOSE, "Chassis Extra: %a\n", mFruRecordInfo[Index]->ChassisExtra[Count]));
      }
    }

    DEBUG ((DEBUG_VERBOSE, "Board Manufacturing date: %d\n", mFruRecordInfo[Index]->ManufacturingDate));
    DEBUG ((DEBUG_VERBOSE, "Board Manufacturer: %a\n", mFruRecordInfo[Index]->BoardManufacturer));
    DEBUG ((DEBUG_VERBOSE, "Board Product: %a\n", mFruRecordInfo[Index]->BoardProduct));
    DEBUG ((DEBUG_VERBOSE, "Board serial: %a\n", mFruRecordInfo[Index]->BoardSerial));
    DEBUG ((DEBUG_VERBOSE, "Board partnum: %a\n", mFruRecordInfo[Index]->BoardPartNum));
    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->BoardExtra[Count]) {
        DEBUG ((DEBUG_VERBOSE, "Board Extra: %a\n", mFruRecordInfo[Index]->BoardExtra[Count]));
      }
    }

    DEBUG ((DEBUG_VERBOSE, "Product Manufacturer: %a\n", mFruRecordInfo[Index]->ProductManufacturer));
    DEBUG ((DEBUG_VERBOSE, "Product Name: %a\n", mFruRecordInfo[Index]->ProductName));
    DEBUG ((DEBUG_VERBOSE, "Product partnum: %a\n", mFruRecordInfo[Index]->ProductPartNum));
    DEBUG ((DEBUG_VERBOSE, "Product Version: %a\n", mFruRecordInfo[Index]->ProductVersion));
    DEBUG ((DEBUG_VERBOSE, "Product Serial: %a\n", mFruRecordInfo[Index]->ProductSerial));
    DEBUG ((DEBUG_VERBOSE, "Product Asset Tag: %a\n", mFruRecordInfo[Index]->ProductAssetTag));
    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->ProductExtra[Count]) {
        DEBUG ((DEBUG_VERBOSE, "Product Extra: %a\n", mFruRecordInfo[Index]->ProductExtra[Count]));
      }
    }
  }
}

/**
  Convert sequence of bytes to hexadecimal string

  @param  IN  Buf  data to convert
  @param  IN  Len  size of data

  @return          Buf representation in hex, possibly truncated to fit
               allocated static memory
**/
CONST CHAR8 *
ConvertRawBytesToString (
  IN CONST UINT8  *RawBytes,
  IN UINT8        Len
  )
{
  static CHAR8  Str[MAXIMUM_BYTES_TO_STRING_SIZE];
  CHAR8         *Cur;
  UINT32        Index;
  UINT32        Size;
  UINT32        Left;

  if (!RawBytes) {
    AsciiSPrint (Str, sizeof (Str), "NULL");
    DEBUG ((DEBUG_INFO, "%a:, returning Null buffer\n", __FUNCTION__));
    return (const char *)Str;
  }

  Cur  = Str;
  Left = sizeof (Str);

  for (Index = 0; Index < Len; Index++) {
    /* may return more than 2, depending on locale */
    Size = AsciiSPrint (Cur, Left, "%2.2x", RawBytes[Index]);
    if (Size >= Left) {
      /* buffer overflow, truncate */
      break;
    }

    Cur  += Size;
    Left -= Size;
  }

  *Cur = '\0';

  return (const char *)Str;
}

/**
  Create a FRURecord for each of the Frus found and update the  DeviceIds and Device Description information

  @Return  Return EFI_SUCCESS if no Ipmi protocol errors are encountered
**/
EFI_STATUS
UpdateFruDeviceIdList (
  VOID
  )
{
  EFI_STATUS                     Status;
  IPMI_GET_SDR_REQUEST           CommandData;
  IPMI_GET_SDR_RESPONSE          *GetSdrResponse;
  UINT32                         ResponseSize;
  IPMI_SDR_RECORD_STRUCT_HEADER  *SdrHeader;
  IPMI_SDR_RECORD_STRUCT_11      *SdrFruRecord;
  IPMI_SENSOR_RECORD_STRUCT      *SdrRecord;
  UINT16                         RecordId;
  UINT8                          RecordType;
  UINT8                          DevIndex;
  UINT8                          ResponseData[36];

  DevIndex       = 0;
  GetSdrResponse = (IPMI_GET_SDR_RESPONSE *)ResponseData;
  SdrHeader      = &(GetSdrResponse->RecordData.SensorHeader);
  SdrRecord      = &(GetSdrResponse->RecordData);
  // IPMI callout to NetFn Storage 0x0A, command 0x23
  //    Request data:
  //      Byte 1,2: Reservation ID
  //      Byte 3,4: Record ID
  //      Byte 5  : Record Offset
  //      Byte 6  : Bytes To Read
  CommandData.ReservationId = 0x0000;
  CommandData.RecordId      = 0x0000;
  CommandData.RecordOffset  = 0x00;
  CommandData.BytesToRead   = sizeof (IPMI_SDR_RECORD_STRUCT_HEADER);

  //    Response data:
  //      Byte 1    : Completion Code
  //      Byte 2,3  : Next Record ID
  //      Byte 4- N : Record Data
  // ResponseData.RecordData.SensorHeader = SdrHeader;
  ResponseSize = sizeof (ResponseData);

  do {
    Status = IpmiSubmitCommand (
               IPMI_NETFN_STORAGE,
               IPMI_STORAGE_GET_SDR,
               (UINT8 *)&CommandData,
               sizeof (CommandData),
               ResponseData,
               &ResponseSize
               );
    // As per the IPMI 2.0 spec, If ‘Record ID’ is specified as 0000h, this command
    // returns the Record Header for the ‘first’ SDR in the repository.
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
      return Status;
    }

    if (GetSdrResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
      DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetSdrResponse->CompletionCode));
      return EFI_PROTOCOL_ERROR;
    }

    // As per the IPMI 2.0 spec, the response should be the SDR record for the requested record ID.
    // check if it is of type SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR, and if yes, fetch the device Id and update the array
    RecordType = SdrHeader->RecordType;
    if (RecordType == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR) {
      // // IPMI callout to NetFn Storage 0x0A, command 0x23, to retrieve complete FRU SDR Record
      CommandData.RecordId    = SdrHeader->RecordId;
      CommandData.BytesToRead = sizeof (IPMI_SDR_RECORD_STRUCT_11);

      // Response data
      ResponseSize = sizeof (ResponseData);
      SetMem (&ResponseData, ResponseSize, 0);

      Status = IpmiSubmitCommand (
                 IPMI_NETFN_STORAGE,
                 IPMI_STORAGE_GET_SDR,
                 (UINT8 *)&CommandData,
                 sizeof (CommandData),
                 ResponseData,
                 &ResponseSize
                 );

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
        return Status;
      }

      if (GetSdrResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
        DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, GetSdrResponse->CompletionCode));
        return EFI_PROTOCOL_ERROR;
      }

      SdrFruRecord             = (IPMI_SDR_RECORD_STRUCT_11 *)&(SdrRecord->SensorType11);
      mFruRecordInfo[DevIndex] = (FRU_DEVICE_INFO *)AllocateZeroPool (sizeof (FRU_DEVICE_INFO));
      if (mFruRecordInfo[DevIndex] == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Memory allocation failed, returning\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      mFruRecordInfo[DevIndex]->FruDeviceId = SdrFruRecord->FruDeviceData.Bits.FruDeviceId;
      CopyMem (mFruRecordInfo[DevIndex]->FruDeviceDescription, SdrFruRecord->String, (UINTN)SdrFruRecord->StringTypeLength.Bits.Length);
      mFruRecordInfo[DevIndex]->FruDeviceDescription[SdrFruRecord->StringTypeLength.Bits.Length] = '\0';

      DevIndex++;
    }

    RecordId = GetSdrResponse->NextRecordId;

    // for each record ID, get SDR record, if it is of type 0x11 then extract FRU Device Id and update
    CommandData.RecordId = RecordId;
    ResponseSize         = sizeof (ResponseData);
    SetMem (&ResponseData, ResponseSize, 0);
  } while ((RecordId != END_OF_SDR_RECORDS) && (DevIndex < MAX_NUMBER_OF_FRU_DEVICE_IDS));

  mRecordCount = DevIndex;
  // Print the list of Fru device IDS with the Device Description
  for (UINT8 i = 0; i < mRecordCount; i++) {
    DEBUG ((DEBUG_INFO, "%a: List of Frus found\n", __FUNCTION__));
    DEBUG ((DEBUG_INFO, "%d \t %a\n", mFruRecordInfo[i]->FruDeviceId, mFruRecordInfo[i]->FruDeviceDescription));
  }

  return EFI_SUCCESS;
}

/**
  Parse one FRU area string from raw data

  @Param  IN  Data     Raw FRU data
  @Param  IN  Offset   Offset into data for area
  @Param  IN  FruLen   Length of the FRU area

  @return              Pointer to FRU area string
**/
CHAR8 *
GetFruAreaStr (
  IN UINT8  *Data,
  IN UINT8  *Offset,
  IN UINT8  FruLen
  )
{
  CONST CHAR8         BcdPlus[] = "0123456789 -.:,_";
  CHAR8               *Str;
  UINT8               i;
  UINT8               j;
  UINT8               k;
  UINT8               Len;
  UINT8               Size;
  UINT8               Index;
  UINT8               Type;
  UINT8               CharIdx;
  SIX_BIT_ASCII_DATA  SixBitAscii;
  CONST CHAR8         *RawBytesToStr;

  Size  = 0;
  Index = *Offset;

  if ((Index >= FruLen) || (Data[Index] == FRU_END_OF_FIELDS)) {
    return NULL;
  }

  // bits 6:7 contain format
  Type = ((Data[Index] & 0xC0) >> 6);

  /* bits 0:5 contain length */
  Len  = Data[Index++];
  Len &= 0x3f;

  switch (Type) {
    case 0:
      /* 00b: binary/unspecified */
      Size = (Len * 2);
      break;
    case 1:
      /* 01b: BCD plus */
      /* hex dump or BCD -> 2x length */
      Size = (Len * 2);
      break;
    case 2:
      /* 10b: 6-bit ASCII */
      /* 4 chars per group of 1-3 bytes, round up to 4 bytes boundary */
      Size = (Len / 3 + 1) * 4;
      break;
    case 3:
      /* 11b: 8-bit ASCII */
      /* no length adjustment */
      Size = Len;
      break;
  }

  if (Size == 0) {
    *Offset = Index;
    return NULL;
  }

  Str = AllocateZeroPool (Size+1);
  if (!Str) {
    return NULL;
  }

  switch (Type) {
    case 0:        /* Binary */
      RawBytesToStr =  ConvertRawBytesToString (&Data[Index], Len);
      if (RawBytesToStr) {
        AsciiStrCpyS (Str, Size+1, RawBytesToStr);
      } else {
        DEBUG ((DEBUG_INFO, "%a: Coversion of raw type0 buffer to string failed\n", __FUNCTION__));
        FreePool (Str);
        return NULL;
      }

      break;
    case 1:        /* BCD plus */
      for (k = 0; k < Size; k++) {
        Str[k] = BcdPlus[((Data[Index + k / 2] >> ((k % 2) ? 0 : 4)) & 0x0f)];
      }

      Str[k] = '\0';
      break;
    case 2:        /* 6-bit ASCII */
      for (i = j = 0; i < Len; i += 3) {
        SixBitAscii.Bits = 0;
        k                = ((Len - i) < 3 ? (Len - i) : 3);
        CopyMem ((void *)&SixBitAscii.Bits, &Data[Index+i], k);
        CharIdx = 0;
        for (k = 0; k < 4; k++) {
          Str[j++]           = ((SixBitAscii.Chars[CharIdx] & 0x3f) + 0x20);
          SixBitAscii.Bits >>= 6;
        }
      }

      Str[j] = '\0';
      break;
    case 3:
      CopyMem (Str, &Data[Index], Size);
      Str[Size] = '\0';
      break;
  }

  Index  += Len;
  *Offset = Index;

  return Str;
}

/**
  Parse FRU Chassis Area contents

  @Param IN *FruChassisArea   Pointer to the Chassis Area Buffer
  @Param IN FruLen            Length of the Chassis Area Buffer
  @Param IN CurrentDevIndex   Device Index of the FruDeviceInfo array

**/
VOID
ParseFruChassisArea (
  IN UINT8  *FruChassisArea,
  IN UINT8  FruLen,
  IN UINT8  CurrentDevIndex
  )
{
  UINT8  Offset;
  UINT8  PrevOffset;
  UINT8  ChassisType;
  CHAR8  *FruString;
  UINT8  Count;

  // skip first two bytes which specify fru area version and fru area length
  Offset                                       = 2;
  ChassisType                                  = (FruChassisArea[Offset] > MAX_VALUE_CHASSIS_TYPE) ? 2 : FruChassisArea[Offset];
  mFruRecordInfo[CurrentDevIndex]->ChassisType = ChassisType;
  Offset++;
  // All Predefined fields in a FRU Specific area should exist as per the FRU spec. Even if the field
  // doesn't exist there is still a placeholder for Type/length byte like XX000000 indicating length is 0.
  // Send a flag PREDEFINED_FIELD to the caller, if it encounters 0 length predefined field, it needs to
  // advance the offset by 1 to parse the next predefined field.
  mFruRecordInfo[CurrentDevIndex]->ChassisPartNum = GetFruAreaStr (FruChassisArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ChassisSerial = GetFruAreaStr (FruChassisArea, &Offset, FruLen);

  // Read any extra customized fields
  // Extra fields may or maynot exist, hence end of fields check is needed.
  for (Count = 0; (Count < MAX_EXTRA_FRU_AREA_ENTRIES); Count++) {
    PrevOffset                                           = Offset;
    mFruRecordInfo[CurrentDevIndex]->ChassisExtra[Count] = GetFruAreaStr (FruChassisArea, &Offset, FruLen);
    if (PrevOffset == Offset) {
      break;
    }
  }

  if (Count == MAX_EXTRA_FRU_AREA_ENTRIES) {
    // Check if there is more extra customized fields
    do {
      PrevOffset = Offset;
      FruString  = GetFruAreaStr (FruChassisArea, &Offset, FruLen);
      if (FruString) {
        DEBUG ((DEBUG_WARN, "%a: Chassis Extra %a\n", FruString));
      }
    } while (PrevOffset != Offset);
  }
}

/**
  Parse FRU Board Area contents

  @Param IN *FruBoardArea     Pointer to the Board Area Buffer
  @Param IN FruLen            Length of the Board Area Buffer
  @Param IN CurrentDevIndex   Device Index of the FruDeviceInfo array

**/
VOID
ParseFruBoardArea (
  IN UINT8  *FruBoardArea,
  IN UINT8  FruLen,
  IN UINT8  CurrentDevIndex
  )
{
  UINT8  Offset;
  UINT8  PrevOffset;
  UINT8  Count;
  CHAR8  *FruString;
  UINT8  Date[3];

  // skip first 3 bytes which specify fru area version and fru area length
  Offset = 3;
  // Copy the next 3 bytes that stores manufacturing date as the number of
  // minutes from 0:00 hrs1/1/96. LS byte first. 00_00_00 means unspecified.
  CopyMem ((VOID *)Date, (VOID *)&FruBoardArea[Offset], sizeof (Date));

  mFruRecordInfo[CurrentDevIndex]->ManufacturingDate = (UINT32)(Date[0] | ((UINT16)Date[1] << 8) | ((UINT32)Date[2] << 16));

  Offset += sizeof (Date);

  // All Predefined fields in a FRU Specific area should exist as per the FRU spec. Even if the field
  // doesn't exist there is still a placeholder for Type/length byte like XX000000 indicating length is 0.
  // Send a flag PREDEFINED_FIELD to the caller, if it encounters 0 length predefined field, it needs to
  // advance the offset by 1 to parse the next predefined field.
  mFruRecordInfo[CurrentDevIndex]->BoardManufacturer = GetFruAreaStr (FruBoardArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->BoardProduct = GetFruAreaStr (FruBoardArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->BoardSerial = GetFruAreaStr (FruBoardArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->BoardPartNum = GetFruAreaStr (FruBoardArea, &Offset, FruLen);

  // Read any extra customized fields
  // Extra fields may or maynot exist, hence end of fields check is needed.
  for (Count = 0; (Count < MAX_EXTRA_FRU_AREA_ENTRIES); Count++) {
    PrevOffset                                         = Offset;
    mFruRecordInfo[CurrentDevIndex]->BoardExtra[Count] = GetFruAreaStr (FruBoardArea, &Offset, FruLen);
    if (PrevOffset == Offset) {
      break;
    }
  }

  if (Count == MAX_EXTRA_FRU_AREA_ENTRIES) {
    // Check if there is more extra customized fields
    do {
      PrevOffset = Offset;
      FruString  = GetFruAreaStr (FruBoardArea, &Offset, FruLen);
      if (FruString) {
        DEBUG ((DEBUG_WARN, "%a: Board Extra %a\n", FruString));
      }
    } while (PrevOffset != Offset);
  }
}

/**
  Parse FRU Product Area contents

  @Param IN *FruProductArea   Pointer to the Product Area Buffer
  @Param IN FruLen            Length of the Product Area Buffer
  @Param IN CurrentDevIndex   Device Index of the FruDeviceInfo array

**/
VOID
ParseFruProductArea (
  IN UINT8  *FruProductArea,
  IN UINT8  FruLen,
  IN UINT8  CurrentDevIndex
  )
{
  UINT8  Offset;
  UINT8  PrevOffset;
  UINT8  Count;
  CHAR8  *FruString;

  // skip first three bytes which specify fru area version, fru area length and fru board language
  Offset = 3;
  // All Predefined fields in a FRU Specific area should exist as per the FRU spec. Even if the field
  // doesn't exist there is still a placeholder for Type/length byte like XX000000 indicating length is 0.
  // Send a flag PREDEFINED_FIELD to the caller, if it encounters 0 length predefined field, it needs to
  // advance the offset by 1 to parse the next predefined field.
  mFruRecordInfo[CurrentDevIndex]->ProductManufacturer = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ProductName = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ProductPartNum = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ProductVersion = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ProductSerial = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  mFruRecordInfo[CurrentDevIndex]->ProductAssetTag = GetFruAreaStr (FruProductArea, &Offset, FruLen);

  // Read any extra customized fields
  // Extra fields may or maynot exist, hence end of fields check is needed.
  for (Count = 0; (Count < MAX_EXTRA_FRU_AREA_ENTRIES); Count++) {
    PrevOffset                                           = Offset;
    mFruRecordInfo[CurrentDevIndex]->ProductExtra[Count] = GetFruAreaStr (FruProductArea, &Offset, FruLen);
    if (Offset == PrevOffset) {
      break;
    }
  }

  if (Count == MAX_EXTRA_FRU_AREA_ENTRIES) {
    // Check if there is more extra customized fields
    do {
      PrevOffset = Offset;
      FruString  = GetFruAreaStr (FruProductArea, &Offset, FruLen);
      if (FruString) {
        DEBUG ((DEBUG_WARN, "%a: Product Extra %a\n", FruString));
      }
    } while (PrevOffset != Offset);
  }
}

/**
  Reads the contents of a specific Fru Area

  @Param IN Offset    Offset in multiples of 8 of a specific Area in the Fru
  @Param IN AreaType  Enum Value of the Type of Area - Chassis/Board/Product
  @Param IN DevIndex  Device Index of the FruDeviceInfo array

  @Return   Returns the EFI_SUCCESS status if no any Ipmi protocol errors are encountered.
**/
EFI_STATUS
ReadSpecificFruArea (
  IN UINT32     Offset,
  IN AREA_TYPE  Type,
  IN UINT8      DevIndex
  )
{
  EFI_STATUS                   Status;
  IPMI_READ_FRU_DATA_REQUEST   CommandData;
  IPMI_READ_FRU_DATA_RESPONSE  *FruData;
  UINT32                       ResponseSize;
  UINT8                        *FruArea;
  UINT8                        *temp;
  UINT32                       FruSize;
  UINT8                        ResponseData[5];
  UINT8                        *FruResponse;

  FruData = (IPMI_READ_FRU_DATA_RESPONSE *)&ResponseData;

  FruSize = 0;
  // First read the first 2 bytes of Chassis Area to get this Area size and then read the Area.
  // IPMI callout to NetFn Storage 0x0A, command 0x11
  //    Request data:
  //      Byte 1  : Device ID
  //      Byte 2,3: Fru Inventory Offset
  //      Byte 4  : Count to Read
  CommandData.DeviceId        = mFruRecordInfo[DevIndex]->FruDeviceId;
  CommandData.InventoryOffset = Offset;
  CommandData.CountToRead     = 2;

  //    Response data:
  //      Byte 1 : Completion Code
  //      Byte 2 : Count returned
  //      Byte 3 : Data[0]
  ResponseSize = sizeof (ResponseData);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_STORAGE,
             IPMI_STORAGE_READ_FRU_DATA,
             (UINT8 *)&CommandData,
             sizeof (CommandData),
             (UINT8 *)&ResponseData,
             &ResponseSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
    return Status;
  }

  if (FruData->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, FruData->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  temp =  FruData->Data;
  // temp[0] = Byte 1 - Chassis/Board/Product Info Area Format Version
  // temp[1] = Byte 2 - Chassis/Board/Product Info Area Size
  FruSize = *(temp + 1) * 8;

  if (FruSize == 0) {
    return EFI_SUCCESS;
  }

  // Read full Fru Chassis/Board/Product Info Area
  CommandData.CountToRead = FruSize;
  ResponseSize            = FruSize + sizeof (IPMI_READ_FRU_DATA_RESPONSE);
  FruResponse             = (UINT8 *)AllocateZeroPool (ResponseSize);
  FruData                 = (IPMI_READ_FRU_DATA_RESPONSE *)FruResponse;

  Status = IpmiSubmitCommand (
             IPMI_NETFN_STORAGE,
             IPMI_STORAGE_READ_FRU_DATA,
             (UINT8 *)&CommandData,
             sizeof (CommandData),
             FruResponse,
             &ResponseSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
    FreePool (FruResponse);
    return Status;
  }

  if (FruData->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, FruData->CompletionCode));
    FreePool (FruResponse);
    return EFI_PROTOCOL_ERROR;
  }

  FruArea = FruData->Data;
  if (Type == CHASSIS_AREA) {
    ParseFruChassisArea (FruArea, FruSize, DevIndex);
  } else if (Type == BOARD_AREA) {
    ParseFruBoardArea (FruArea, FruSize, DevIndex);
  } else if (Type == PRODUCT_AREA) {
    ParseFruProductArea (FruArea, FruSize, DevIndex);
  }

  // TODO: Should we parse Multi record Area.
  // Currently it is not parsed as there is no known usecase.

  FreePool (FruResponse);

  return EFI_SUCCESS;
}

/**
  Parses the Fru header to see what areas are present and calls the specific functions
  to parse the Area contents.

  @Param IN DevId       Device Id of the Fru to parse
  @Param IN DevIndex    Device Index of the FruDeviceInfo array

  @Return            Returns if EFI_SUCCESS if no Ipmi Protocol errors are encountered.
**/
EFI_STATUS
ReadFruHeader (
  IN UINT8  DevId,
  IN UINT8  DevIndex
  )
{
  EFI_STATUS                   Status;
  IPMI_READ_FRU_DATA_REQUEST   CommandData;
  IPMI_READ_FRU_DATA_RESPONSE  *FruReadResponse;
  UINT32                       ResponseSize;
  UINT8                        ResponseData[16];
  FRU_HEADER                   *Header;

  FruReadResponse = (IPMI_READ_FRU_DATA_RESPONSE *)&ResponseData;
  Header          = (FRU_HEADER *)&FruReadResponse->Data;
  // IPMI callout to NetFn Storage 0x0A, command 0x11
  //    Request data:
  //      Byte 1  : Device ID
  //      Byte 2,3: Fru Inventory Offset
  //      Byte 4  : Count to Read
  CommandData.DeviceId        = DevId;
  CommandData.InventoryOffset = 0x0000;
  CommandData.CountToRead     = sizeof (FRU_HEADER);

  //    Response data:
  //      Byte 1 : Completion Code
  //      Byte 2 : Count returned
  //      Byte 3 : Data[0]
  ResponseSize = sizeof (ResponseData);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_STORAGE,
             IPMI_STORAGE_READ_FRU_DATA,
             (UINT8 *)&CommandData,
             sizeof (CommandData),
             (UINT8 *)&ResponseData,
             &ResponseSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
    return Status;
  }

  if (FruReadResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, FruReadResponse->CompletionCode));
    return EFI_PROTOCOL_ERROR;
  }

  if (Header->Version != 1) {
    DEBUG ((DEBUG_ERROR, "%a: Unknown FRU Header Version, Returning: 0x%x\n", __FUNCTION__, Header->Version));
    return EFI_PROTOCOL_ERROR;
  }

  // Print the header data
  // Each of the area offsets are converted in to bytes and printed
  DEBUG ((DEBUG_VERBOSE, "%a: FRU Area Offsets for Device Id: %d\n", __FUNCTION__, DevId));
  DEBUG ((DEBUG_VERBOSE, " Header.Version = 0x%x\n", Header->Version));
  DEBUG ((DEBUG_VERBOSE, " Internal Area Offset = 0x%x\n", Header->Offset.Internal * 8));
  DEBUG ((DEBUG_VERBOSE, " Chassis Area Offset = 0x%x\n", Header->Offset.Chassis * 8));
  DEBUG ((DEBUG_VERBOSE, " Board Area Offset = 0x%x\n", Header->Offset.Board * 8));
  DEBUG ((DEBUG_VERBOSE, " Product Area Offset = 0x%x\n", Header->Offset.Product * 8));
  DEBUG ((DEBUG_VERBOSE, " Multi Record Area Offset = 0x%x\n", Header->Offset.Multi * 8));

  // If a specific area is not present in the Fru data, the area offset will be set to 0x00
  // Read FRU Chassis Area
  if (Header->Offset.Chassis * 8 >= sizeof (FRU_HEADER)) {
    ReadSpecificFruArea (Header->Offset.Chassis * 8, CHASSIS_AREA, DevIndex);
  }

  // Read FRU Board Area
  if (Header->Offset.Board * 8  >= sizeof (FRU_HEADER)) {
    ReadSpecificFruArea (Header->Offset.Board * 8, BOARD_AREA, DevIndex);
  }

  // Read FRU Product Area
  if (Header->Offset.Product * 8 >= sizeof (FRU_HEADER)) {
    ReadSpecificFruArea (Header->Offset.Product * 8, PRODUCT_AREA, DevIndex);
  }

  return EFI_SUCCESS;
}

/**
  Read the contents of each Fru with in the list of the Device Ids

  @Return                Returns if EFI_SUCCESS if no Ipmi Protocol errors or the
                         out of resource errors  are encountered.
**/
EFI_STATUS
ReadFru (
  VOID
  )
{
  EFI_STATUS                                 Status;
  IPMI_GET_FRU_INVENTORY_AREA_INFO_REQUEST   CommandData;
  IPMI_GET_FRU_INVENTORY_AREA_INFO_RESPONSE  *FruInventoryInfo;
  UINT32                                     ResponseSize;
  UINT8                                      DevIndex;
  UINT16                                     FruSize;
  UINT8                                      ResponseData[8];

  FruInventoryInfo =  (IPMI_GET_FRU_INVENTORY_AREA_INFO_RESPONSE *)&ResponseData;
  for (DevIndex = 0; DevIndex < mRecordCount; DevIndex++) {
    // for each of the device ID in the list, Read the FRU data and populate the structure fields.
    // Get the FRU inventory Area Information
    // IPMI callout to NetFn Storage 0x0A, command 0x10
    //    Request data:
    //      Byte 1: Device ID
    CommandData.DeviceId = mFruRecordInfo[DevIndex]->FruDeviceId;

    //    Response data:
    //      Byte 1    : Completion Code
    //      Byte 2,3  : Inventory Area Size
    //      Byte 4    : Access Type
    ResponseSize = sizeof (ResponseData);

    Status = IpmiSubmitCommand (
               IPMI_NETFN_STORAGE,
               IPMI_STORAGE_GET_FRU_INVENTORY_AREAINFO,
               (UINT8 *)&CommandData,
               sizeof (CommandData),
               (UINT8 *)&ResponseData,
               &ResponseSize
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
      return Status;
    }

    if (FruInventoryInfo->CompletionCode != IPMI_COMP_CODE_NORMAL) {
      DEBUG ((DEBUG_ERROR, "%a: Completion code = 0x%x. Returning\n", __FUNCTION__, FruInventoryInfo->CompletionCode));
      return EFI_PROTOCOL_ERROR;
    }

    FruSize = FruInventoryInfo->InventoryAreaSize;

    if (FruSize < 1) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid FRU Size : %d\n", __FUNCTION__, FruSize));
      continue;
    }

    // Read the FRU HEADER, 8 byte size and continue reading fru data if area offsets are valid.
    ReadFruHeader (mFruRecordInfo[DevIndex]->FruDeviceId, DevIndex);
  }

  PrintRecords ();
  return EFI_SUCCESS;
}

/**
  ReadAllFrus
  The Functions Calls the FRU reader functions to get the
  Platform information.

  @param  FruInfo     The pointer to the list of FRU records
  @param  FruCount    The pointer to the param that stores the total Fru Records read

  @retval EFI_SUCCESS          Returns if all the Frus are parsed and the FruDeviceInfo[] is updated.
          EFI_OUT_OF_RESOURCES Returns if dynamic memory allocation fails for any buffer.
          EFI_PROTOCOL_ERROR   Returns if Ipmi protocol error occurs.
**/
EFI_STATUS
ReadAllFrus (
  OUT FRU_DEVICE_INFO  ***FruInfo,
  OUT UINT8            *FruCount
  )
{
  EFI_STATUS  Status;

  if ((FruCount == NULL) || (FruInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid FruInfo or FrusCount pointer\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = UpdateFruDeviceIdList ();
  if ( Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from UpdateFruDeviceIdList()", __FUNCTION__, Status));
    return Status;
  }

  Status = ReadFru ();
  if ( Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from ReadFru()", __FUNCTION__, Status));
    return Status;
  }

  *FruInfo  = mFruRecordInfo;
  *FruCount = mRecordCount;

  return EFI_SUCCESS;
}

/**
 FreeAllFruRecords frees the memory for all the Fru Record buffers.

 @retval EFI_SUCCESS        always Returns EFI_SUCCESS
**/
EFI_STATUS
FreeAllFruRecords (
  VOID
  )
{
  UINT8  Index;
  UINT8  Count;

  for (Index = 0; Index < mRecordCount; Index++) {
    if (mFruRecordInfo[Index]->ChassisPartNum) {
      FreePool (mFruRecordInfo[Index]->ChassisPartNum);
    }

    if (mFruRecordInfo[Index]->ChassisSerial) {
      FreePool (mFruRecordInfo[Index]->ChassisSerial);
    }

    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->ChassisExtra[Count]) {
        FreePool (mFruRecordInfo[Index]->ChassisExtra[Count]);
      }
    }

    if (mFruRecordInfo[Index]->BoardManufacturer) {
      FreePool (mFruRecordInfo[Index]->BoardManufacturer);
    }

    if (mFruRecordInfo[Index]->BoardProduct) {
      FreePool (mFruRecordInfo[Index]->BoardProduct);
    }

    if (mFruRecordInfo[Index]->BoardSerial) {
      FreePool (mFruRecordInfo[Index]->BoardSerial);
    }

    if (mFruRecordInfo[Index]->BoardPartNum) {
      FreePool (mFruRecordInfo[Index]->BoardPartNum);
    }

    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->BoardExtra[Count]) {
        FreePool (mFruRecordInfo[Index]->BoardExtra[Count]);
      }
    }

    if (mFruRecordInfo[Index]->ProductManufacturer) {
      FreePool (mFruRecordInfo[Index]->ProductManufacturer);
    }

    if (mFruRecordInfo[Index]->ProductName) {
      FreePool (mFruRecordInfo[Index]->ProductName);
    }

    if (mFruRecordInfo[Index]->ProductPartNum) {
      FreePool (mFruRecordInfo[Index]->ProductPartNum);
    }

    if (mFruRecordInfo[Index]->ProductVersion) {
      FreePool (mFruRecordInfo[Index]->ProductVersion);
    }

    if (mFruRecordInfo[Index]->ProductSerial) {
      FreePool (mFruRecordInfo[Index]->ProductSerial);
    }

    if (mFruRecordInfo[Index]->ProductAssetTag) {
      FreePool (mFruRecordInfo[Index]->ProductAssetTag);
    }

    for (Count = 0; Count < MAX_EXTRA_FRU_AREA_ENTRIES; Count++) {
      if (mFruRecordInfo[Index]->ProductExtra[Count]) {
        FreePool (mFruRecordInfo[Index]->ProductExtra[Count]);
      }
    }

    FreePool (mFruRecordInfo[Index]);
  }

  return EFI_SUCCESS;
}
