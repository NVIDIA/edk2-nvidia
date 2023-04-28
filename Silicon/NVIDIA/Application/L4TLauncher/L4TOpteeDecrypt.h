/** @file
  The API and structures for UEFI payloads decryption.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef L4T_OPTEE_DECRYPT_H_
#define L4T_OPTEE_DECRYPT_H_

#include <Library/OpteeNvLib.h>

#define TA_CPUBL_PAYLOAD_DECRYPTION_UUID  { 0x0e35e2c9, 0xb329, 0x4ad9, { 0xa2, 0xf5, 0x8c, 0xa9, 0xbb, 0xbd, 0x77, 0x13 } }

#define EKB_USER_KEY_KERNEL_ENCRYPTION  1

#define CPUBL_PAYLOAD_DECRYPTION_CMD_IS_IMAGE_DECRYPT_ENABLE  0
#define CPUBL_PAYLOAD_DECRYPTION_CMD_DECRYPT_IMAGES           1

#define JETSON_CPUBL_PAYLOAD_DECRYPTION_INIT    1
#define JETSON_CPUBL_PAYLOAD_DECRYPTION_UPDATE  2
#define JETSON_CPUBL_PAYLOAD_DECRYPTION_FINAL   3

#define BCH_BINARY_LEN_OFFSET  0x1404

/* Default BCH Image Header Size is 8K */
#define BOOT_COMPONENT_HEADER_SIZE  SIZE_8KB
/* Default decryption init block size is BOOT_COMPONENT_HEADER_SIZE */
#define OPTEE_DECRYPT_INIT_BLOCK_SIZE  BOOT_COMPONENT_HEADER_SIZE
/* Set the default decryption update block size to 2M Bytes */
#define OPTEE_DECRYPT_UPDATE_BLOCK_SIZE  SIZE_2MB

typedef struct {
  UINTN                  TotalSize;
  UINTN                  CommBufSize;
  VOID                   *OpteeMsgArgPa;
  VOID                   *OpteeMsgArgVa;
  VOID                   *CommBufPa;
  VOID                   *CommBufVa;
  OPTEE_SHM_COOKIE       *MsgCookiePa;
  OPTEE_SHM_COOKIE       *MsgCookieVa;
  OPTEE_SHM_PAGE_LIST    *ShmListPa;
  OPTEE_SHM_PAGE_LIST    *ShmListVa;
} OPTEE_SESSION;

EFI_STATUS
EFIAPI
IsImageEncryptionEnable (
  OUT BOOLEAN  *Enabled
  );

EFI_STATUS
EFIAPI
OpteeDecryptImage (
  IN EFI_FILE_HANDLE        *Handle OPTIONAL,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo OPTIONAL,
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo OPTIONAL,
  IN UINT64                 SrcFileSize,
  OUT VOID                  **DstBuffer,
  OUT UINT64                *DstFileSize
  );

#endif /* L4T_OPTEE_DECRYPT_H_ */
