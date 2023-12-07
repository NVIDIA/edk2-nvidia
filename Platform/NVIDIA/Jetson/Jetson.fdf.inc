#
# SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


!include Features/Ext4Pkg/Ext4.fdf.inc
!include Silicon/NVIDIA/Drivers/NvGopDriver/NvGopDriver.fdf.inc

  #
  # Tegra UART driver
  #
  INF Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf

  #
  # Platform Configuration Manager Data
  #
!ifdef CONFIG_CONFIG_MANAGER
  INF Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
  INF Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
!endif

  #
  # Regulator support
  #
  INF Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # FDT Support
  #
!ifdef CONFIG_DEVICETREE
  !ifdef CONFIG_ACPI
    INF EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf
  !else
    INF Silicon/NVIDIA/Drivers/DtOnlyPlatformDxe/DtOnlyPlatformDxe.inf
  !endif
!endif

  #
  # PinMux Support
  #
  INF Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf

  #
  # SE RNG
  #
  INF Silicon/NVIDIA/Tegra/T194/Drivers/SeRngDxe/SeRngDxe.inf
  INF Silicon/NVIDIA/Tegra/T234/Drivers/SeRngDxe/SeRngDxe.inf

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
  # Tegra PWM clock driver
  #
  INF Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

  #
  # EQOS NIC Support
  #
  INF Silicon/NVIDIA/Drivers/EqosDeviceDxe/EqosDeviceDxe.inf

  #
  # VPR Support
  #
  INF Silicon/NVIDIA/Drivers/VPRDxe/VPRDxe.inf

  #
  # Golden Register Support
  #
  INF Silicon/NVIDIA/Drivers/GoldenRegisterDxe/GoldenRegisterDxe.inf

  #
  # FSI Support
  #
  INF Silicon/NVIDIA/Drivers/FSIDxe/FSIDxe.inf

  #
  # NV Non-Discoverable PCI Device
  #
  INF Silicon/NVIDIA/Drivers/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  #
  # NV Display Controller Driver
  #
  INF Silicon/NVIDIA/Drivers/NvDisplayControllerDxe/NvDisplayControllerDxe.inf

  #
  # T194 Display support
  #
  INF Silicon/NVIDIA/Tegra/T194/Drivers/T194GraphicsOutputDxe/T194GraphicsOutputDxe.inf

  #
  # MM Communication
  #
  INF Silicon/NVIDIA/Drivers/MmCommunicationOpteeDxe/MmCommunicationOptee.inf

  #
  # FW Partition
  #
  INF Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe/FwPartitionNorFlashDxe.inf
  INF Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe/FwPartitionBlockIoDxe.inf
  INF Silicon/NVIDIA/Drivers/FwImageDxe/FwImageDxe.inf
  INF Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf

  #
  # SMBIOS
  #
!ifdef CONFIG_SMBIOS
  INF ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  INF Silicon/NVIDIA/Drivers/SmbiosOemDxe/SmbiosMiscOemDxe/SmbiosMiscOemDxe.inf
  INF ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
!endif

  #
  # BasicProfiler Support
  #
  INF Silicon/NVIDIA/Drivers/BasicProfilerDxe/BasicProfilerDxe.inf

  #
  # TPM
  #
  INF SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf
  INF SecurityPkg/Tcg/Tcg2Config/Tcg2ConfigDxe.inf

  #
  # Capsule application (EFI Services)
  #
  INF MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf

  #
  # Android Fastboot
  #
  INF Silicon/NVIDIA/Drivers/TegraUsbDeviceControllerDxe/TegraUsbDeviceControllerDxe.inf
  INF EmbeddedPkg/Application/AndroidFastboot/AndroidFastbootApp.inf
  INF EmbeddedPkg/Drivers/AndroidFastbootTransportUsbDxe/FastbootTransportUsbDxe.inf
  INF Silicon/NVIDIA/Drivers/TegraFastBootDxe/TegraFastBootDxe.inf
