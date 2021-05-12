#
#  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

[Defines]
  SKUID_IDENTIFIER            = ALL
  PLATFORM_VERSION            = 0.1
  DSC_SPECIFICATION           = 0x00010005
  SUPPORTED_ARCHITECTURES     = AARCH64
  BUILD_TARGETS               = DEBUG|RELEASE|NOOPT

  #
  # Define ESRT GUIDs for Firmware Management Protocol instances
  #
  DEFINE SYSTEM_FMP_ESRT_GUID = 7C374309-1649-4682-8BEE-04F3A8399414

!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]

  TegraCombinedSerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
  Tegra16550SerialPortLib|Silicon/NVIDIA/Library/Tegra16550SerialPortLib/Tegra16550SerialPortLib.inf
  TegraSbsaSerialPortLib|Silicon/NVIDIA/Library/TegraSbsaSerialPortLib/TegraSbsaSerialPortLib.inf
  SerialPortLib|Silicon/NVIDIA/Library/TegraSerialPortLib/TegraSerialPortLib.inf

  RealTimeClockLib|Silicon/NVIDIA/Library/MaximRealTimeClockLib/MaximRealTimeClockLib.inf

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsPatchableInModule]
  # Console Resolution (Full HD)
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|0

[PcdsDynamicDefault.common.T186]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x03881000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x03882000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA186'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(d33335fe-a16c-4765-a04d-f3c78999e580)}
  gNVIDIATokenSpaceGuid.PcdBootImgSigningHeaderSize|0x190
  gNVIDIATokenSpaceGuid.PcdSdhciSDR104Disable|TRUE
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000

[PcdsDynamicDefault.common.T194]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x03881000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x03882000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA194'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID(be3f5d68-7654-4ed2-838c-2a2faf901a78)}
  gNVIDIATokenSpaceGuid.PcdBootImgSigningHeaderSize|0x1000
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gNVIDIATokenSpaceGuid.PcdSdhciCoherentDMADisable|TRUE
  gNVIDIATokenSpaceGuid.PcdBPMPPCIeControllerEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdFloorsweepCpus|TRUE

[PcdsDynamicHii.common.T194]
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpi|L"PCIeInOsConfig"|gNVIDIATokenSpaceGuid|0x0|1|BS


################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
  #
  # Tegra Platform Configuration
  #
  Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # Tegra UART driver
  #
  Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf

  #
  # Platform Configuration Manager Data
  #
  Silicon/NVIDIA/Jetson/T186/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      PlatformAslTablesLib|Silicon/NVIDIA/Jetson/T186/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Jetson/T186/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT
  }
  Silicon/NVIDIA/Jetson/T194/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      PlatformAslTablesLib|Silicon/NVIDIA/Jetson/T194/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Jetson/T194/Drivers/ConfigurationManager/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT -I$(BIN_DIR)/Silicon/NVIDIA/Library/ASLTemplateTablesLib/ASLTemplateTablesLib/OUTPUT
  }

  #
  # Platform USB FW
  #
  Silicon/NVIDIA/Jetson/T186/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf
  Silicon/NVIDIA/Jetson/T194/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf

  #
  # BPMP-FW IPC protocol
  #
  Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf

  Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf

  #
  # Regulator support
  #
  Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

  #
  # FDT Support
  #
  EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf {
    <LibraryClasses>
      DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbKernelLoaderLib.inf
  }

  #
  # ACPI Support
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/PlatformHasAcpiLib/PlatformHasAcpiLib.inf
  }

  # Boot support for mkbootimg partitions
  Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf

  #
  # QSPI Support
  #
  Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

  #
  # EMMC/SD
  #
  Silicon/NVIDIA/Drivers/SdMmcControllerDxe/SdMmcControllerDxe.inf
  MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
  MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
  MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf

  #
  # SE RNG
  #
  Silicon/NVIDIA/Jetson/T194/Drivers/SeRngDxe/SeRngDxe.inf

  #
  # PCI Support
  #
  ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
  Silicon/NVIDIA/Drivers/TegraP2UDxe/TegraP2UDxe.inf
  Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf

  # FVB Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
  }

  #
  # Usb Support
  #
  Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf
  Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf
  Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf

  #
  # Tegra Pwm clock driver
  #
  Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

  #
  # Tegra Controller Enable Driver
  #
  Silicon/NVIDIA/Drivers/TegraControllerEnableDxe/TegraControllerEnableDxe.inf

  #
  # Tegra I2C Support
  #
  Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf

  #
  # Tegra EEPROM Support
  #
  Silicon/NVIDIA/Drivers/CvmEepromDxe/CvmEeprom.inf

  #Security drivers
  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
  SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf

  #
  # EQOS NIC Support
  #
  Silicon/NVIDIA/Drivers/EqosDeviceDxe/EqosDeviceDxe.inf

  #
  # VPR Support
  #
  Silicon/NVIDIA/Drivers/VPRDxe/VPRDxe.inf

  #
  # Golden Register Support
  #
  Silicon/NVIDIA/Drivers/GoldenRegisterDxe/GoldenRegisterDxe.inf

  #
  # FSI Support
  #
  Silicon/NVIDIA/Drivers/FSIDxe/FSIDxe.inf
