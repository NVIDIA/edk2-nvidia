#
# Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

  #
  # Serial Console driver
  #
  INF MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # Platform Configuration Manager Data
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf

  #
  # BPMP-FW IPC protocol
  #
  INF Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  INF Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf
  INF Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  INF Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  INF Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  INF Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf

  #
  # Regulator support
  #
  INF Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # Fuse Support
  #
  INF Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf

  #
  # QSPI Support
  #
  INF Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  INF Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

  #
  # FVB Support
  #
  INF Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  INF Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf

  #
  # PCI Support
  #
  INF ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  INF Silicon/NVIDIA/Server/TH500/Drivers/PcieControllerDxe/PcieControllerDxe.inf

  #
  # Memory Tests
  #
  INF MdeModulePkg/Universal/MemoryTest/GenericMemoryTestDxe/GenericMemoryTestDxe.inf

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
  # Report status code
  #
  INF Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/OemDescStatusCodeDxe.inf

  #
  # TPM
  #
  INF Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf
  INF SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf
  INF SecurityPkg/Tcg/Tcg2Config/Tcg2ConfigDxe.inf
