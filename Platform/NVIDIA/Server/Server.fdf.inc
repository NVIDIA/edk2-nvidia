#
# SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

  #
  # Ramloaded OS
  #
  INF Silicon/NVIDIA/Drivers/RamDiskOS/RamDiskOS.inf

  #
  # APEI
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/ApeiDxe/Apei.inf

  #
  # MM Communication
  #
  INF Silicon/NVIDIA/Drivers/MmCommunicateFfaDxe/MmCommunicationFfa.inf

  #
  # TPM
  #
  INF Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf
  INF SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf
  INF Silicon/NVIDIA/Drivers/Tcg2Config/Tcg2ConfigDxe.inf
  INF MdeModulePkg/Universal/SmbiosMeasurementDxe/SmbiosMeasurementDxe.inf

  #
  # User Authentication
  #
  INF Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationDxe.inf

  #
  # GH100 GPU Driver
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/TH500GpuDxe/TH500GpuDxe.inf

  #
  # FPGA I2C Support
  #
  INF Silicon/NVIDIA/Drivers/FpgaI2c/FpgaI2c.inf
