#
# Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

  #
  # Serial Console driver
  #
  INF MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # IPMI Null Driver
  #
  INF Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Platform Configuration Manager Data
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf

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
  INF Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf

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

!ifdef $(ENABLE_INTELUNDI)
  #
  # Intel 500 series NIC
  #
  INF IntelUndiPkg/XGigUndiDxe/XGigUndiDxe.inf
!endif

  #
  # BMC Virtual NIC support
  #
  INF Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.inf

  #
  # Realtek USB NIC
  #
  INF Drivers/Realtek/Bus/Usb/UsbNetworking/RtkUsbUndiDxe.inf

  #
  # NS ERoT
  #
  INF Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiDxe.inf

  #
  # MM MCTP
  #
  INF Silicon/NVIDIA/Drivers/MctpMmDxe/MctpMmDxe.inf
