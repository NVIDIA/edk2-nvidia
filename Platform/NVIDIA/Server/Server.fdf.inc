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

  #
  # User Authentication
  #
  INF Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationDxe.inf

  #
  # GH100 GPU Driver
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/TH500GpuDxe/TH500GpuDxe.inf

  #
  # Redfish Host Interface Support
  #
  INF RedfishPkg/RedfishHostInterfaceDxe/RedfishHostInterfaceDxe.inf

  #
  # AST2600 VGA GOP Support
  FILE DRIVER = b403dcf9-8b43-44f7-b9ea-383eb33e548d {
    SECTION PE32 = Drivers/ASpeed/ASpeedGopBinPkg/AArch64/ASpeedAst2600Gop.efi
  }

  #
  # Default TLS Cipher Suites
  #
  INF Silicon/NVIDIA/Drivers/RedfishCipherDxe/RedfishCipherDxe.inf

  #
  # Redfish support
  #
  INF RedfishPkg/RestJsonStructureDxe/RestJsonStructureDxe.inf
  INF RedfishPkg/RedfishRestExDxe/RedfishRestExDxe.inf
  INF RedfishPkg/RedfishCredentialDxe/RedfishCredentialDxe.inf
  INF RedfishPkg/RedfishDiscoverDxe/RedfishDiscoverDxe.inf
  INF RedfishPkg/RedfishConfigHandler/RedfishConfigHandlerDriver.inf
  INF RedfishPkg/RedfishPlatformConfigDxe/RedfishPlatformConfigDxe.inf
  INF MdeModulePkg/Universal/RegularExpressionDxe/RegularExpressionDxe.inf
  INF RedfishClientPkg/RedfishFeatureCoreDxe/RedfishFeatureCoreDxe.inf
  INF RedfishClientPkg/RedfishETagDxe/RedfishETagDxe.inf
  INF RedfishClientPkg/RedfishConfigLangMapDxe/RedfishConfigLangMapDxe.inf
  INF RedfishClientPkg/Features/ComputerSystem/v1_17_0/Dxe/ComputerSystemDxe.inf
  INF RedfishClientPkg/Features/ComputerSystemCollectionDxe/ComputerSystemCollectionDxe.inf
  INF RedfishClientPkg/Features/Bios/v1_2_0/Dxe/BiosDxe.inf
  INF RedfishClientPkg/Converter/ComputerSystem/v1_17_0/RedfishComputerSystem_V1_17_0_Dxe.inf
  INF RedfishClientPkg/Converter/ComputerSystemCollection/RedfishComputerSystemCollection_Dxe.inf
  INF RedfishClientPkg/Converter/Bios/v1_2_0/RedfishBios_V1_2_0_Dxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishBiosDxe/PlatformRedfishBiosDxe.inf
