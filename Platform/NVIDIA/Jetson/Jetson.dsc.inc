#
#  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2013-2018, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

[Defines]
  SKUID_IDENTIFIER            = ALL
  PLATFORM_VERSION            = 0.1
  DSC_SPECIFICATION           = 0x00010005
  SUPPORTED_ARCHITECTURES     = AARCH64
  BUILD_TARGETS               = DEBUG|RELEASE|NOOPT

!include config.dsc.inc
!include Platform/NVIDIA/NVIDIA.common.dsc.inc

[LibraryClasses.common]
  SsdtPcieSupportLib|DynamicTablesPkg/Library/Common/SsdtPcieSupportLib/SsdtPcieSupportLib.inf

  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLibNull/DxeTcg2PhysicalPresenceLibNull.inf
  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLibNull/PeiDxeTpmPlatformHierarchyLib.inf
  Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibTcg2/Tpm2DeviceLibTcg2.inf
  Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
  Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################
[Components.common]
  #
  # SE RNG
  #
  Silicon/NVIDIA/Tegra/T194/Drivers/SeRngDxe/SeRngDxe.inf
  Silicon/NVIDIA/Tegra/T234/Drivers/SeRngDxe/SeRngDxe.inf

  #
  # PinMux Support
  #
  Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf

  #
  # Tegra Pwm clock driver
  #
  Silicon/NVIDIA/Drivers/TegraPwmDxe/TegraPwmDxe.inf

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

  #
  # Default Variables Device Tree
  #
  Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree.inf

  #
  # MM Communication
  #
  Silicon/NVIDIA/Drivers/MmCommunicationOpteeDxe/MmCommunicationOptee.inf {
    <LibraryClasses>
      NULL|StandaloneMmPkg/Library/VariableMmDependency/VariableMmDependency.inf
      NULL|Silicon/NVIDIA/Library/StandaloneMmUnavailableLib/StandaloneMmUnavailableLib.inf
  }

  #
  # SMBIOS
  #
!ifdef CONFIG_SMBIOS
  ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  Silicon/NVIDIA/Drivers/SmbiosOemDxe/SmbiosMiscOemDxe/SmbiosMiscOemDxe.inf
  ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
!endif

  #
  # BasicProfiler Support
  #
  Silicon/NVIDIA/Drivers/BasicProfilerDxe/BasicProfilerDxe.inf

  #
  # TPM
  #
  SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|Silicon/NVIDIA/Library/OpteeTpmDeviceLib/OpteeTpmDeviceLib.inf
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha384/HashInstanceLibSha384.inf
  }
  SecurityPkg/Tcg/Tcg2Config/Tcg2ConfigDxe.inf

  #
  # Android Fastboot
  #
  Silicon/NVIDIA/Drivers/TegraUsbDeviceControllerDxe/TegraUsbDeviceControllerDxe.inf
  EmbeddedPkg/Application/AndroidFastboot/AndroidFastbootApp.inf
  EmbeddedPkg/Drivers/AndroidFastbootTransportUsbDxe/FastbootTransportUsbDxe.inf
  Silicon/NVIDIA/Drivers/TegraFastBootDxe/TegraFastBootDxe.inf
