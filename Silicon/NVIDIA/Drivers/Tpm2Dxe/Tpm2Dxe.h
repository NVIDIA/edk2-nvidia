/** @file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TPM2_DXE_H__
#define __TPM2_DXE_H__

#define TPM2_SIGNATURE  SIGNATURE_32('T','P','M','S')

#define TPM_SPI_ADDR_PREFIX  0xD4
#define TPM_SPI_CMD_SIZE     4

typedef struct {
  UINT32                             Signature;
  EFI_HANDLE                         QspiControllerHandle;
  EFI_HANDLE                         Tpm2Handle;
  EFI_DEVICE_PATH_PROTOCOL           *TpmDevicePath;
  BOOLEAN                            ProtocolsInstalled;

  NVIDIA_QSPI_CONTROLLER_PROTOCOL    *QspiController;
  UINT8                              ChipSelect;

  NVIDIA_TPM2_PROTOCOL               Tpm2Protocol;
} TPM2_PRIVATE_DATA;

#define TPM2_PRIVATE_DATA(a)  CR(a, TPM2_PRIVATE_DATA, Tpm2Protocol, TPM2_SIGNATURE)

#endif
