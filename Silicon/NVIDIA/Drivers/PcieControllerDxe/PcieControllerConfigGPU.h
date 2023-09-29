/** @file

  PCIe Controller GPU specific configuration

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
   Configure Nvidia GPUs for 256B MPS and enable 10-bit tags if supported

   @param [in]  PciIo       PCIe handle for the GPU device

   @retval TRUE  Configuration successful
   @retval FALSE Failed to configure the GPU
*/
EFI_STATUS
PcieConfigGPUDevice (
  EFI_PCI_IO_PROTOCOL  *PciIo
  );
