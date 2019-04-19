#
#  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
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

!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]

  RealTimeClockLib|Silicon/NVIDIA/Library/MaximRealTimeClockLib/MaximRealTimeClockLib.inf

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsPatchableInModule]
  # Console Resolution (Full HD)
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|1920
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|1080

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
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
  EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf

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
  # FVB Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf

  #
  # Usb Support
  #
  Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf
  Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf

  #
  # Android Boot Support
  #
  Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf

