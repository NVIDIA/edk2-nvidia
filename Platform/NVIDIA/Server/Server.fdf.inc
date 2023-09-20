#
# SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
!ifdef CONFIG_CONFIG_MANAGER
  INF Silicon/NVIDIA/Server/TH500/Drivers/ConfigurationManager/ConfigurationManagerData/ConfigurationManagerDataDxe.inf
!endif

  #
  # Regulator support
  #
  INF Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf
  INF Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf

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
  INF Silicon/NVIDIA/Drivers/Tcg2Config/Tcg2ConfigDxe.inf

!ifdef $(ENABLE_INTELUNDI)
  #
  # Intel 500 series NIC
  #
  INF IntelUndiPkg/XGigUndiDxe/XGigUndiDxe.inf
  #
  # Intel 700 series NIC
  #
  INF IntelUndiPkg/I40eUndiDxe/I40eUndiDxe.inf
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
  INF Silicon/NVIDIA/Drivers/RedfishHostInterfaceControlDxe/RedfishHostInterfaceControlDxe.inf

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
  INF RedfishClientPkg/RedfishHttpDxe/RedfishHttpDxe.inf
  INF RedfishClientPkg/Features/ComputerSystem/v1_17_0/Dxe/ComputerSystemDxe.inf
  INF RedfishClientPkg/Features/ComputerSystemCollectionDxe/ComputerSystemCollectionDxe.inf
  INF RedfishClientPkg/Features/Bios/v1_2_0/Dxe/BiosDxe.inf
  INF RedfishClientPkg/Features/BootOptionCollection/BootOptionCollectionDxe.inf
  INF RedfishClientPkg/Features/BootOption/v1_0_4/Dxe/BootOptionDxe.inf
  INF RedfishClientPkg/Converter/ComputerSystem/v1_17_0/RedfishComputerSystem_V1_17_0_Dxe.inf
  INF RedfishClientPkg/Converter/ComputerSystemCollection/RedfishComputerSystemCollection_Dxe.inf
  INF RedfishClientPkg/Converter/SecureBoot/v1_1_0/RedfishSecureBoot_V1_1_0_Dxe.inf
  INF RedfishClientPkg/Converter/Bios/v1_2_0/RedfishBios_V1_2_0_Dxe.inf
  INF RedfishClientPkg/Converter/BootOptionCollection/RedfishBootOptionCollection_Dxe.inf
  INF RedfishClientPkg/Converter/BootOption/v1_0_4/RedfishBootOption_V1_0_4_Dxe.inf
  INF RedfishClientPkg/RedfishBootstrapAccountDxe/RedfishBootstrapAccountDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishBiosDxe/PlatformRedfishBiosDxe.inf
  INF Silicon/NVIDIA/Drivers/RedfishFirmwareInfoDxe/RedfishFirmwareInfoDxe.inf
  INF RedfishClientPkg/Features/SecureBoot/v1_1_0/Dxe/SecureBootDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishBootDxe/PlatformRedfishBootDxe.inf
  INF Silicon/NVIDIA/Drivers/PlatformRedfishOverrideDxe/PlatformRedfishOverrideDxe.inf
  INF RedfishClientPkg/Features/SecureBootDatabaseCollectionDxe/SecureBootDatabaseCollectionDxe.inf
  INF RedfishClientPkg/Converter/SecureBootDatabaseCollection/RedfishSecureBootDatabaseCollection_Dxe.inf
  INF RedfishClientPkg/Features/SecureBootDatabase/v1_0_1/Dxe/SecureBootDatabaseDxe.inf
  INF RedfishClientPkg/Converter/SecureBootDatabase/v1_0_1/RedfishSecureBootDatabase_V1_0_1_Dxe.inf
  INF RedfishClientPkg/RedfishTaskServiceDxe/RedfishTaskServiceDxe.inf
  INF RedfishClientPkg/RedfishSecureBootKeysDxe/RedfishSecureBootKeysDxe.inf

  #
  # FPGA I2C Support
  #
  INF Silicon/NVIDIA/Drivers/FpgaI2c/FpgaI2c.inf
