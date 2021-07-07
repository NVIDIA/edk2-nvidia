#
#  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#


  #
  # Tegra UART driver
  #
  INF Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf

  #
  # Platform Configuration Manager Data
  #
  INF Silicon/NVIDIA/Tegra/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
  INF Silicon/NVIDIA/Jetson/T234/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf

  #
  # Platform USB FW
  #
  INF Silicon/NVIDIA/Tegra/T194/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf
  INF Silicon/NVIDIA/Jetson/T234/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf

  #
  # BPMP-FW IPC protocol
  #
  INF Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  INF Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf
  INF Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  INF MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
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
  INF Silicon/NVIDIA/Jetson/T234/Drivers/SeRngDxe/SeRngDxe.inf

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
  # Tegra I2C Support
  #
  INF Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf

  #
  # Tegra EEPROM Support
  #
  INF Silicon/NVIDIA/Drivers/CvmEepromDxe/CvmEeprom.inf

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
