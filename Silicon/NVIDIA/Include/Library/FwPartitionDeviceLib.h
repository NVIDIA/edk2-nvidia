/** @file

  FW Partition Device Library

  Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_PARTITION_DEVICE_LIB_H__
#define __FW_PARTITION_DEVICE_LIB_H__

#include <Library/DevicePathLib.h>
#include <Protocol/FwPartitionProtocol.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define MAX_FW_PARTITIONS                    80
#define FW_PARTITION_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('F','W','P','A')

typedef struct _FW_PARTITION_PRIVATE_DATA  FW_PARTITION_PRIVATE_DATA;
typedef struct _FW_PARTITION_DEVICE_INFO   FW_PARTITION_DEVICE_INFO;

/**
  Convert address for runtime execution.

  @param[in]  Pointer           Pointer to address to convert

  @retval None

**/
typedef
VOID
(EFIAPI *FW_PARTITION_ADDRESS_CONVERT)(
  IN  VOID **Pointer
  );

/**
  Read data from device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to read from
  @param[in]  Bytes             Number of bytes to read
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_PARTITION_DEVICE_READ)(
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  OUT VOID                              *Buffer
  );

/**
  Write data to device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in] Offset             Offset to write
  @param[in] Bytes              Number of bytes to write
  @param[in] Buffer             Address of write data

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_PARTITION_DEVICE_WRITE)(
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  );

// device information structure
struct _FW_PARTITION_DEVICE_INFO {
  CONST CHAR16                 *DeviceName;
  FW_PARTITION_DEVICE_READ     DeviceRead;
  FW_PARTITION_DEVICE_WRITE    DeviceWrite;
  UINT32                       BlockSize;
};

// partition information structure
typedef struct {
  CHAR16     Name[FW_PARTITION_NAME_LENGTH];
  UINTN      Bytes;
  UINT64     Offset;
  BOOLEAN    IsActivePartition;
} FW_PARTITION_INFO;

// fw partition private data structure
struct _FW_PARTITION_PRIVATE_DATA {
  UINT32                          Signature;

  // Partition info
  FW_PARTITION_INFO               PartitionInfo;

  // Device info
  FW_PARTITION_DEVICE_INFO        *DeviceInfo;

  // Protocol info
  EFI_HANDLE                      Handle;
  NVIDIA_FW_PARTITION_PROTOCOL    Protocol;
};

/**
  Add new FW partition.  Initializes a FW_PARTITION_PRIVATE_DATA structure
  for the partition.

  @param[in]  Name              Partition name
  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset of partition in device
  @param[in]  Bytes             Size of partition in bytes

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionAdd (
  IN  CONST CHAR16              *Name,
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes
  );

/**
  Add new FW partitions for all partitions in the device's secondary GPT.
  Initializes a FW_PARTITION_PRIVATE_DATA structure for each partition.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  DeviceSizeInBytes Size of device in bytes

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionAddFromDeviceGpt (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    DeviceSizeInBytes
  );

/**
  Add new FW partitions for all partitions in the given partition table.
  Initializes a FW_PARTITION_PRIVATE_DATA structure for each partition.

  @param[in]  GptHeader         Pointer to the GPT header structure
  @param[in]  PartitionTable    Pointer to the partition table entry array
  @param[in]  DeviceInfo        Pointer to device info struct

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionAddFromPartitionTable (
  IN  CONST EFI_PARTITION_TABLE_HEADER  *GptHeader,
  IN  EFI_PARTITION_ENTRY               *PartitionTable,
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo
  );

/**
  Convert all pointer addresses within FwPartitionDeviceLib to support
  runtime execution.

  @param[in]  ConvertFunction   Function used to convert a pointer

  @retval None

**/
VOID
EFIAPI
FwPartitionAddressChangeHandler (
  IN  FW_PARTITION_ADDRESS_CONVERT  ConvertFunction
  );

/**
  Check that given Offset and Bytes don't exceed given MaxOffset.

  @param[in]  MaxOffset         Maximum offset allowed
  @param[in]  Offset            Offset of operation
  @param[in]  Bytes             Number of bytes to access at offset

  @retval EFI_SUCCESS           Offset and Bytes are within the MaxOffset limit
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionCheckOffsetAndBytes (
  IN  UINT64  MaxOffset,
  IN  UINT64  Offset,
  IN  UINTN   Bytes
  );

/**
  De-initialize the FwPartitionDeviceLib, freeing all resources.
  The caller should uninstall any installed protocols before calling
  this function.

  @retval None

**/
VOID
EFIAPI
FwPartitionDeviceLibDeinit (
  VOID
  );

/**
  Initialize the FwPartitionDeviceLib.

  @param[in]  ActiveBootChain   The active FW boot chain (0=a, 1=b)
  @param[in]  MaxFwPartitions   Maximum number of FW partitions to support

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionDeviceLibInit (
  IN  UINT32   ActiveBootChain,
  IN  UINTN    MaxFwPartitions,
  IN  BOOLEAN  OverwriteActiveFwPartition
  );

/**
  Find the FW_PARTITION_PRIVATE_DATA structure for the given partition name.

  @param[in]  Name              Partition name

  @retval NULL                  Partition name not found
  @retval non-NULL              Pointer to the partition's data structure

**/
FW_PARTITION_PRIVATE_DATA *
EFIAPI
FwPartitionFindByName (
  IN  CONST CHAR16  *Name
  );

/**
  Get the number of initialized FW_PARTITION_PRIVATE_DATA structures.

  @retval UINTN                 Number of initialized structures

**/
UINTN
EFIAPI
FwPartitionGetCount (
  VOID
  );

/**
  Get a pointer to the first element of the FW_PARTITION_PRIVATE_DATA array.

  @retval FW_PARTITION_PRIVATE_DATA     Pointer to first data structure

**/
FW_PARTITION_PRIVATE_DATA *
EFIAPI
FwPartitionGetPrivateArray (
  VOID
  );

#endif
