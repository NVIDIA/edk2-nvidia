/** @file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TPM2_DEVICE_LIB_INTERNAL_H__
#define __TPM2_DEVICE_LIB_INTERNAL_H__

#include <Protocol/Tpm2.h>

#define TPM_SPI_ADDR_PREFIX  0xD4

#define TPM_MAX_SPI_FRAMESIZE  64
#define TPM_SPI_CMD_SIZE       4

//
// Locality 0
//
#define TPM_ACCESS_0           0x0000
#define TPM_INT_ENABLE_0       0x0008
#define TPM_INT_VECTOR_0       0x000C
#define TPM_INT_STATUS_0       0x0010
#define TPM_INTF_CAPABILITY_0  0x0014
#define TPM_STS_0              0x0018
#define TPM_STS_BURSTCNT_LO_0  0x0019
#define TPM_STS_BURSTCNT_HI_0  0x001A
#define TPM_STS_MISC_0         0x001B
#define TPM_DATA_FIFO_0        0x0024
#define TPM_XDATA_FIFO_0       0x0080
#define TPM_DID_VID_0          0x0F00
#define TPM_RID_0              0x0F04

#define TIS_PC_STS_MISC_CANCEL  (TIS_PC_STS_CANCEL >> 24)

/**
  Send a command to TPM for execution and return response data.

  @param[in]      Tpm2          Pointer to NVIDIA_TPM2_PROTOCOL
  @param[in]      BufferIn      Buffer for command data.
  @param[in]      SizeIn        Size of command data.
  @param[in, out] BufferOut     Buffer for response data.
  @param[in, out] SizeOut       Size of response data.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_BUFFER_TOO_SMALL  Response data buffer is too small.
  @retval EFI_DEVICE_ERROR      Unexpected device behavior.
  @retval EFI_UNSUPPORTED       Unsupported TPM version
**/
EFI_STATUS
TisTpmCommand (
  IN     NVIDIA_TPM2_PROTOCOL  *Tpm2,
  IN     UINT8                 *BufferIn,
  IN     UINT32                SizeIn,
  IN OUT UINT8                 *BufferOut,
  IN OUT UINT32                *SizeOut
  );

/**
  Get the control of TPM chip by sending requestUse command TIS_PC_ACC_RQUUSE
  to ACCESS Register in the time of default TIS_TIMEOUT_A.

  @param[in] Tpm2             Pointer to NVIDIA_TPM2_PROTOCOL

  @retval    EFI_SUCCESS      Get the control of TPM chip.
  @retval    EFI_NOT_FOUND    TPM chip doesn't exit.
  @retval    EFI_TIMEOUT      Can't get the TPM control in time.
**/
EFI_STATUS
TisRequestUseTpm (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2
  );

#endif /* __TPM2_DEVICE_LIB_INTERNAL_H__ */
