#
# Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  INF Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
  INF Silicon/NVIDIA/Tegra/T234/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf

  #
  # Platform USB FW
  #
  INF Silicon/NVIDIA/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf

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
  INF Silicon/NVIDIA/Drivers/I2cExpanderGpio/I2cExpanderGpio.inf
  INF Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf

  #
  # Regulator support
  #
  INF Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # FDT Support
  #
  INF EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf


  #
  # Usb Support
  #
  INF Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf
  INF Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf
  INF Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  INF Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  INF Silicon/NVIDIA/Drivers/XusbControllerDxe/XusbControllerDxe.inf
  INF Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf

  #
  # SD MMC
  #
  INF Silicon/NVIDIA/Drivers/SdMmcControllerDxe/SdMmcControllerDxe.inf
  INF MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
  INF MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
  INF MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf

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
  # Android Boot Support
  #
  INF Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf

  #
  # PCI Support
  #
  INF ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  INF Silicon/NVIDIA/Drivers/TegraP2UDxe/TegraP2UDxe.inf
  INF Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf

  #
  # FVB Support
  #
  INF Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  INF Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf


  INF SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
  INF SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  INF SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf

  #
  # Tegra PWM clock driver
  #
  INF Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

  #
  # Tegra Controller Enable Driver
  #
  INF Silicon/NVIDIA/Drivers/TegraControllerEnableDxe/TegraControllerEnableDxe.inf

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
  # UFS support
  #
  INF Silicon/NVIDIA/Drivers/UfsControllerDxe/UfsControllerDxe.inf
  INF MdeModulePkg/Bus/Pci/UfsPciHcDxe/UfsPciHcDxe.inf
  INF MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf

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
  # BasicProfiler Support
  #
  INF Silicon/NVIDIA/Drivers/BasicProfilerDxe/BasicProfilerDxe.inf
