#
#  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2011-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent

!include Platform/NVIDIA/NVIDIA.global.dsc.inc

[Defines]
  SKUID_IDENTIFIER            = ALL
  PLATFORM_VERSION            = 0.1
  DSC_SPECIFICATION           = 0x00010005
  SUPPORTED_ARCHITECTURES     = AARCH64
  BUILD_TARGETS               = DEBUG|RELEASE|NOOPT
  OUTPUT_DIRECTORY            = Build/$(PLATFORM_NAME)

!include config.dsc.inc

# Value defines are parsed even if there is a guarding ifdef, define some defaults
!ifndef CONFIG_BOOT_DEFAULT_TIMEOUT
CONFIG_BOOT_DEFAULT_TIMEOUT=0
!endif
!ifndef CONFIG_MEMORY_TEST_ITERATIONS
CONFIG_MEMORY_TEST_ITERATIONS=1
!endif
!ifndef CONFIG_MEMORY_TEST_BIT_FADE_PATTERN
CONFIG_MEMORY_TEST_BIT_FADE_PATTERN=0xAA55AA55AA55AA55
!endif
!ifndef CONFIG_MEMORY_TEST_BIT_FADE_WAIT
CONFIG_MEMORY_TEST_BIT_FADE_WAIT=0
!endif
!ifndef CONFIG_MEMORY_TEST_BIT_FADE_WAIT
CONFIG_MEMORY_TEST_BIT_FADE_WAIT=0
!endif
!ifndef CONFIG_FIRMWARE_CAPSULE_SIZE
CONFIG_FIRMWARE_CAPSULE_SIZE=0
!endif
!ifndef CONFIG_ARM_WATCHDOG_CONTROL_BASE
CONFIG_ARM_WATCHDOG_CONTROL_BASE=0
!endif
!ifndef CONFIG_ARM_WATCHDOG_REFRESH_BASE
CONFIG_ARM_WATCHDOG_REFRESH_BASE=0
!endif
!ifndef CONFIG_ARM_WATCHDOG_INTERRUPT
CONFIG_ARM_WATCHDOG_INTERRUPT=0
!endif

  #
  # Define Image Type ID GUIDs for Firmware Management Protocol
  #
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T194 = be3f5d68-7654-4ed2-838c-2a2faf901a78
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T234 = bf0d4599-20d4-414e-b2c5-3595b1cda402
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA = 8655e5cf-297b-4213-84d5-b6817203a432
  DEFINE FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500        = $(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA)
!ifdef CONFIG_FIRMWARE_MANAGEMENT
  DEFINE SYSTEM_FMP_ESRT_GUID = $(CONFIG_FIRMWARE_MANAGEMENT_FMP_ESRT_GUID)
!else
  # Should never be used only defined to resolve parsing issues.
  DEFINE SYSTEM_FMP_ESRT_GUID = 004ad128-93d8-43a7-9fad-4ac77a931035
!endif

[SkuIds]
  0|DEFAULT
!ifdef CONFIG_SOC_XAVIER
  1|T194
!endif
!ifdef CONFIG_SOC_ORIN
  2|T234
  3|T234SLT|T234
!endif
!ifdef CONFIG_SOC_GRACE
  5|TH500
  253|TH500Presil|TH500
!endif
!ifdef CONFIG_SOC_ORIN
  255|T234Presil|T234
!endif

[BuildOptions.common.EDKII.DXE_CORE,BuildOptions.common.EDKII.DXE_DRIVER,BuildOptions.common.EDKII.UEFI_DRIVER,BuildOptions.common.EDKII.UEFI_APPLICATION]
  GCC:*_*_*_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000

[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  GCC:*_*_ARM_DLINK_FLAGS = -Wl,-z,common-page-size=0x1000
  GCC:*_*_AARCH64_DLINK_FLAGS = -Wl,-z,common-page-size=0x10000

[BuildOptions.common]
  GCC:*_*_*_PLATFORM_FLAGS = -fstack-protector-strong

[BuildOptions]
  *_*_*_ASL_FLAGS = -tc -li -so -oi
  GCC:*_*_*_CC_FLAGS = -D ENABLE_MD5_DEPRECATED_INTERFACES

!ifdef CONFIG_FS_EXT4
!include Features/Ext4Pkg/Ext4.dsc.inc
!endif
!ifdef CONFIG_NVIDIA_DISPLAY
!include Silicon/NVIDIA/Drivers/NvGopDriver/NvGopDriver.dsc.inc
!endif

[LibraryClasses.common]
  DebugLib|Silicon/NVIDIA/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  DebugLogScratchRegLib|Silicon/NVIDIA/Library/DebugLogScratchRegLib/DebugLogScratchRegLib.inf
!ifdef CONFIG_DEBUG_DYNAMIC_PRINT_LEVEL
  # Configurable DebugLib
  DebugPrintErrorLevelLib|Silicon/NVIDIA/Library/DebugPrintErrorLevelLib/DebugPrintErrorLevelLib.inf
!else
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
!endif

  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  MmUnblockMemoryLib|MdePkg/Library/MmUnblockMemoryLib/MmUnblockMemoryLibNull.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  PrePiHobListPointerLib|ArmPlatformPkg/Library/PrePiHobListPointerLib/PrePiHobListPointerLib.inf

  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  VariableFlashInfoLib|MdeModulePkg/Library/BaseVariableFlashInfoLib/BaseVariableFlashInfoLib.inf

  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf

!if $(TARGET) != RELEASE
  UnitTestLib|UnitTestFrameworkPkg/Library/UnitTestLib/UnitTestLib.inf
  UnitTestPersistenceLib|UnitTestFrameworkPkg/Library/UnitTestPersistenceLibNull/UnitTestPersistenceLibNull.inf
  UnitTestResultReportLib|UnitTestFrameworkPkg/Library/UnitTestResultReportLib/UnitTestResultReportLibDebugLib.inf
!endif

  #
  # Assume everything is fixed at build
  #
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf

  BaseMemoryLib|MdePkg/Library/BaseMemoryLibOptDxe/BaseMemoryLibOptDxe.inf

  # Networking Requirements
  NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
  DpcLib|NetworkPkg/Library/DxeDpcLib/DxeDpcLib.inf
  UdpIoLib|NetworkPkg/Library/DxeUdpIoLib/DxeUdpIoLib.inf
  IpIoLib|NetworkPkg/Library/DxeIpIoLib/DxeIpIoLib.inf
  TcpIoLib|NetworkPkg/Library/DxeTcpIoLib/DxeTcpIoLib.inf
  HttpIoLib|NetworkPkg/Library/DxeHttpIoLib/DxeHttpIoLib.inf

  # HttpLib is used for Http Boot
  HttpLib|NetworkPkg/Library/DxeHttpLib/DxeHttpLib.inf

  TlsLib|CryptoPkg/Library/TlsLib/TlsLib.inf
  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf

  ArmTrngLib|ArmPkg/Library/ArmTrngLib/ArmTrngLib.inf
  ArmMonitorLib|ArmPkg/Library/ArmMonitorLib/ArmMonitorLib.inf
  ArmHvcLib|ArmPkg/Library/ArmHvcLib/ArmHvcLib.inf

  # ARM Architectural Libraries
  CacheMaintenanceLib|ArmPkg/Library/ArmCacheMaintenanceLib/ArmCacheMaintenanceLib.inf
  DefaultExceptionHandlerLib|ArmPkg/Library/DefaultExceptionHandlerLib/DefaultExceptionHandlerLib.inf
!ifdef CONFIG_DEBUG_LOG_SCRATCH_REG
  DefaultExceptionCallbackLib|Silicon/NVIDIA/Server/TH500/Library/DefaultExceptionCallbackLibServer/DefaultExceptionCallbackLibServer.inf
!else
  DefaultExceptionCallbackLib|ArmPkg/Library/DefaultExceptionCallbackLibNull/DefaultExceptionCallbackLibNull.inf
!endif
  CpuExceptionHandlerLib|ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf
  ArmDisassemblerLib|ArmPkg/Library/ArmDisassemblerLib/ArmDisassemblerLib.inf
  ArmGicLib|ArmPkg/Drivers/ArmGic/ArmGicLib.inf
  ArmGicArchLib|ArmPkg/Library/ArmGicArchLib/ArmGicArchLib.inf
  ArmSmcLib|ArmPkg/Library/ArmSmcLib/ArmSmcLib.inf
  ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerPhyCounterLib/ArmGenericTimerPhyCounterLib.inf
  ResetSystemLib|ArmPkg/Library/ArmSmcPsciResetSystemLib/ArmSmcPsciResetSystemLib.inf
  ArmLib|ArmPkg/Library/ArmLib/ArmBaseLib.inf
  ArmMmuLib|ArmPkg/Library/ArmMmuLib/ArmMmuBaseLib.inf
  OpteeNvLib|Silicon/NVIDIA/Library/OpteeNvLib/OpteeNvLib.inf

  TimerLib|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf

  #
  # Uncomment (and comment out the next line) For RealView Debugger. The Standard IO window
  # in the debugger will show load and unload commands for symbols. You can cut and paste this
  # into the command window to load symbols. We should be able to use a script to do this, but
  # the version of RVD I have does not support scripts accessing system memory.
  #
  #PeCoffExtraActionLib|ArmPkg/Library/RvdPeCoffExtraActionLib/RvdPeCoffExtraActionLib.inf
  PeCoffExtraActionLib|ArmPkg/Library/DebugPeCoffExtraActionLib/DebugPeCoffExtraActionLib.inf
  #PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf

  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  DebugAgentTimerLib|EmbeddedPkg/Library/DebugAgentTimerLibNull/DebugAgentTimerLibNull.inf

  SemihostLib|ArmPkg/Library/SemihostLib/SemihostLib.inf

  # BDS Libraries
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  PlatformBootManagerLib|Silicon/NVIDIA/Library/PlatformBootManagerLib/PlatformBootManagerLib.inf
!ifdef CONFIG_IPMI_BMC
  PlatformBootOrderLib|Silicon/NVIDIA/Library/PlatformBootOrderLib/PlatformBootOrderLib.inf
!else
  PlatformBootOrderLib|Silicon/NVIDIA/Library/PlatformBootOrderLib/PlatformBootOrderLibNoIpmi.inf
!endif
  BootLogoLib|MdeModulePkg/Library/BootLogoLib/BootLogoLib.inf

  AcpiLib|EmbeddedPkg/Library/AcpiLib/AcpiLib.inf

  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

  #
  # Secure Boot dependencies
  #
  IntrinsicLib|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  OpensslLib|CryptoPkg/Library/OpensslLib/OpensslLibFull.inf
  TpmMeasurementLib|SecurityPkg/Library/DxeTpmMeasurementLib/DxeTpmMeasurementLib.inf
!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  AuthVariableLib|SecurityPkg/Library/AuthVariableLib/AuthVariableLib.inf
!else
  AuthVariableLib|MdeModulePkg/Library/AuthVariableLibNull/AuthVariableLibNull.inf
!endif
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/BaseCryptLib.inf
  RngLib|MdeModulePkg/Library/BaseRngLibTimerLib/BaseRngLibTimerLib.inf

  PlatformSecureLib|SecurityPkg/Library/PlatformSecureLibNull/PlatformSecureLibNull.inf
  VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
  VarSetCallbacksLib|MdeModulePkg/Library/VarSetCallbacksLibNull/VarSetCallbacksLibNull.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
  PlatformPKProtectionLib|SecurityPkg/Library/PlatformPKProtectionLibVarPolicy/PlatformPKProtectionLibVarPolicy.inf
  SecureBootVariableLib|SecurityPkg/Library/SecureBootVariableLib/SecureBootVariableLib.inf
  SecureBootVariableProvisionLib|SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf

  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibFmp/DxeCapsuleLib.inf
  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf

  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf

  TegraPlatformInfoLib|Silicon/NVIDIA/Library/TegraPlatformInfoLib/TegraPlatformInfoLib.inf
  PlatformResourceLib|Silicon/NVIDIA/Library/PlatformResourceLib/PlatformResourceLib.inf
  SystemResourceLib|Silicon/NVIDIA/Library/SystemResourceLib/SystemResourceLib.inf
  GoldenRegisterLib|Silicon/NVIDIA/Library/GoldenRegisterLib/GoldenRegisterLib.inf
  GptLib|Silicon/NVIDIA/Library/GptLib/GptLib.inf
  DramCarveoutLib|Silicon/NVIDIA/Library/DramCarveoutLib/DramCarveoutLib.inf
  BootChainInfoLib|Silicon/NVIDIA/Library/BootChainInfoLib/BootChainInfoLib.inf
  ConfigurationManagerLib|Silicon/NVIDIA/Library/ConfigurationManagerLib/ConfigurationManagerLib.inf

  OemMiscLib|Silicon/NVIDIA/Library/OemMiscLib/OemMiscLib.inf
  SmbiosMiscLib|Silicon/NVIDIA/Library/SmbiosMiscLib/SmbiosMiscLib.inf
  TegraCpuFreqHelperLib|Silicon/NVIDIA/Library/TegraCpuFreqHelperLib/TegraCpuFreqHelper.inf
  TimeBaseLib|EmbeddedPkg/Library/TimeBaseLib/TimeBaseLib.inf
  FruLib|Silicon/NVIDIA/Library/FruLib/FruLib.inf

  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbLoaderLib.inf

  DeviceDiscoveryDriverLib|Silicon/NVIDIA/Library/DeviceDiscoveryDriverLib/DeviceDiscoveryDriverLib.inf
  DeviceDiscoveryLib|Silicon/NVIDIA/Library/DeviceDiscoveryLib/DeviceDiscoveryLib.inf

  TegraDeviceTreeOverlayLib|Silicon/NVIDIA/Library/TegraDeviceTreeOverlayLib/TegraDeviceTreeOverlayLib.inf
  TegraDeviceTreeKernelOverlayLib|Silicon/NVIDIA/Library/TegraDeviceTreeOverlayLib/TegraDeviceTreeKernelOverlayLib.inf

  QspiControllerLib|Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLib.inf
  RedfishPlatformHostInterfaceOemLib|Silicon/NVIDIA/Library/RedfishPlatformHostInterfaceOemLib/RedfishPlatformHostInterfaceOemLib.inf

  # USB Requirements
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  DmaLib|EmbeddedPkg/Library/NonCoherentDmaLib/NonCoherentDmaLib.inf
  UsbFalconLib|Silicon/NVIDIA/Library/UsbFalconLib/UsbFalconLib.inf

  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  BmpSupportLib|MdeModulePkg/Library/BaseBmpSupportLib/BaseBmpSupportLib.inf

  FmpAuthenticationLib|SecurityPkg/Library/FmpAuthenticationLibPkcs7/FmpAuthenticationLibPkcs7.inf
  FmpPayloadHeaderLib|FmpDevicePkg/Library/FmpPayloadHeaderLibV1/FmpPayloadHeaderLibV1.inf
  CapsuleUpdatePolicyLib|Silicon/NVIDIA/Library/CapsuleUpdatePolicyLibOnProtocol/CapsuleUpdatePolicyLibOnProtocol.inf
  FmpDependencyLib|FmpDevicePkg/Library/FmpDependencyLib/FmpDependencyLib.inf
  FmpDependencyCheckLib|FmpDevicePkg/Library/FmpDependencyCheckLibNull/FmpDependencyCheckLibNull.inf
  FmpDependencyDeviceLib|FmpDevicePkg/Library/FmpDependencyDeviceLibNull/FmpDependencyDeviceLibNull.inf
  FwPartitionDeviceLib|Silicon/NVIDIA/Library/FwPartitionDeviceLib/FwPartitionDeviceLib.inf
  BrBctUpdateDeviceLib|Silicon/NVIDIA/Library/BrBctUpdateDeviceLib/BrBctUpdateDeviceLib.inf
  FwImageLib|Silicon/NVIDIA/Library/FwImageLib/FwImageLib.inf
  FwPackageLib|Silicon/NVIDIA/Library/FwPackageLib/FwPackageLib.inf
  VerPartitionLib|Silicon/NVIDIA/Library/VerPartitionLib/VerPartitionLib.inf
  FmpParamLib|Silicon/NVIDIA/Library/FmpParamLib/FmpParamLib.inf
!ifdef CONFIG_FIRMWARE_MANAGEMENT_DIRECT
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpDeviceLib/FmpDeviceLib.inf
!endif
!ifdef CONFIG_FIRMWARE_MANAGEMENT_EROT
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpErotLib/FmpErotLib.inf

  ErotQspiLib|Silicon/NVIDIA/Library/ErotQspiLib/ErotQspiLib.inf
  PldmBaseLib|Silicon/NVIDIA/Library/PldmBaseLib/PldmBaseLib.inf
  PldmFwUpdateLib|Silicon/NVIDIA/Library/PldmFwUpdateLib/PldmFwUpdateLib.inf
  PldmFwUpdatePkgLib|Silicon/NVIDIA/Library/PldmFwUpdatePkgLib/PldmFwUpdatePkgLib.inf
  PldmFwUpdateTaskLib|Silicon/NVIDIA/Library/PldmFwUpdateTaskLib/PldmFwUpdateTaskLib.inf
  ErotLib|Silicon/NVIDIA/Library/ErotLib/ErotLib.inf
!endif
!ifdef CONFIG_FIRMWARE_MANAGEMENT_BLOB
  FmpDeviceLib|Silicon/NVIDIA/Library/FmpBlobLib/FmpBlobLib.inf
!endif

  AmlLib|DynamicTablesPkg/Library/Common/AmlLib/AmlLib.inf
  SsdtSerialPortFixupLib|Silicon/NVIDIA/Library/SsdtSerialPortFixupLib/SsdtSerialPortFixupLib.inf
  TableHelperLib|DynamicTablesPkg/Library/Common/TableHelperLib/TableHelperLib.inf
  AcpiHelperLib|DynamicTablesPkg/Library/Common/AcpiHelperLib/AcpiHelperLib.inf
  SmbiosStringTableLib|DynamicTablesPkg/Library/Common/SmbiosStringTableLib/SmbiosStringTableLib.inf
  JedecJep106Lib|MdePkg/Library/JedecJep106Lib/JedecJep106Lib.inf

  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  PciHostBridgeLib|Silicon/NVIDIA/Library/PciHostBridgeLib/PciHostBridgeLib.inf

  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf

  PL011UartLib|ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf
  PL011UartClockLib|ArmPlatformPkg/Library/PL011UartClockLib/PL011UartClockLib.inf
  TegraCombinedSerialPortLib|Silicon/NVIDIA/Library/TegraCombinedSerialPort/TegraCombinedSerialPortLib.inf
  Tegra16550SerialPortLib|Silicon/NVIDIA/Library/Tegra16550SerialPortLib/Tegra16550SerialPortLib.inf
  TegraSbsaSerialPortLib|Silicon/NVIDIA/Library/TegraSbsaSerialPortLib/TegraSbsaSerialPortLib.inf
  SerialPortLib|Silicon/NVIDIA/Library/TegraSerialPortLib/TegraSerialPortLib.inf

  MceAriLib|Silicon/NVIDIA/Library/MceAriLib/MceAriLib.inf
  NvgLib|Silicon/NVIDIA/Library/NvgLib/NvgLib.inf
  FloorSweepingLib|Silicon/NVIDIA/Library/FloorSweepingLib/FloorSweepingLib.inf
  DeviceTreeHelperLib|Silicon/NVIDIA/Library/DeviceTreeHelperLib/DeviceTreeHelperLib.inf
  DisplayDeviceTreeHelperLib|Silicon/NVIDIA/Library/DisplayDeviceTreeHelperLib/DisplayDeviceTreeHelperLib.inf

  Crc8Lib|Silicon/NVIDIA/Library/Crc8Lib/Crc8Lib.inf

!ifdef CONFIG_IPMI_BMC
  IpmiBaseLib|IpmiFeaturePkg/Library/IpmiBaseLib/IpmiBaseLib.inf
  IpmiCommandLib|IpmiFeaturePkg/Library/IpmiCommandLib/IpmiCommandLib.inf
!endif
  MctpBaseLib|Silicon/NVIDIA/Library/MctpBaseLib/MctpBaseLib.inf
  MctpNvVdmLib|Silicon/NVIDIA/Library/MctpNvVdmLib/MctpNvVdmLib.inf

!ifdef CONFIG_REDFISH
  #
  # Redfish support
  #
  RestExLib|RedfishPkg/Library/DxeRestExLib/DxeRestExLib.inf
  Ucs2Utf8Lib|RedfishPkg/Library/BaseUcs2Utf8Lib/BaseUcs2Utf8Lib.inf
  BaseSortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf
  RedfishCrtLib|RedfishPkg/PrivateLibrary/RedfishCrtLib/RedfishCrtLib.inf
  JsonLib|RedfishPkg/Library/JsonLib/JsonLib.inf
  RedfishLib|RedfishPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
  HiiUtilityLib|RedfishPkg/Library/HiiUtilityLib/HiiUtilityLib.inf
  RedfishPlatformConfigLib|RedfishPkg/Library/RedfishPlatformConfigLib/RedfishPlatformConfigLib.inf
  RedfishDebugLib|RedfishPkg/Library/RedfishDebugLib/RedfishDebugLib.inf
  RedfishHttpLib|RedfishClientPkg/Library/RedfishHttpLib/RedfishHttpLib.inf
  RedfishAddendumLib|RedfishClientPkg/Library/RedfishAddendumLib/RedfishAddendumLib.inf

  RedfishLib|RedfishClientPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
  RedfishFeatureUtilityLib|RedfishClientPkg/Library/RedfishFeatureUtilityLib/RedfishFeatureUtilityLib.inf
  RedfishContentCodingLib|RedfishPkg/Library/RedfishContentCodingLibNull/RedfishContentCodingLibNull.inf
  ConverterCommonLib|RedfishClientPkg/ConverterLib/edk2library/ConverterCommonLib/ConverterCommonLib.inf
  RedfishResourceIdentifyLib|RedfishClientPkg/Library/RedfishResourceIdentifyLibNull/RedfishResourceIdentifyLibNull.inf
  EdkIIRedfishResourceConfigLib|RedfishClientPkg/Library/EdkIIRedfishResourceConfigLib/EdkIIRedfishResourceConfigLib.inf
  RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  RedfishVersionLib|RedfishClientPkg/Library/RedfishVersionLib/RedfishVersionLib.inf
  RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
  ComputerSystemV1_17_0Lib|RedfishClientPkg/ConverterLib/edk2library/ComputerSystem/v1_17_0/Lib.inf
  ComputerSystemV1_22_0Lib|RedfishClientPkg/ConverterLib/edk2library/ComputerSystem/v1_22_0/Lib.inf
  ComputerSystemCollectionLib|RedfishClientPkg/ConverterLib/edk2library/ComputerSystemCollection/Lib.inf
  BiosV1_2_0Lib|RedfishClientPkg/ConverterLib/edk2library/Bios/v1_2_0/Lib.inf
  SecureBootV1_1_0Lib|RedfishClientPkg/ConverterLib/edk2library/SecureBoot/v1_1_0/Lib.inf
  BootOptionCollectionLib|RedfishClientPkg/ConverterLib/edk2library/BootOptionCollection/Lib.inf
  BootOptionV1_0_4Lib|RedfishClientPkg/ConverterLib/edk2library/BootOption/v1_0_4/Lib.inf
  SecureBootDatabaseCollectionLib|RedfishClientPkg/ConverterLib/edk2library/SecureBootDatabaseCollection/Lib.inf
  SecureBootDatabaseV1_0_1Lib|RedfishClientPkg/ConverterLib/edk2library/SecureBootDatabase/v1_0_1/Lib.inf
  RedfishJsonBlobLib|RedfishClientPkg/Library/RedfishJsonBlobLib/RedfishJsonBlobLib.inf
  RedfishSecureBootKeysLib|RedfishClientPkg/Library/RedfishSecureBootKeysLib/RedfishSecureBootKeysLib.inf
  RedfishMessageLib|RedfishClientPkg/Library/RedfishMessageLib/RedfishMessageLib.inf
  RedfishTaskLib|Silicon/NVIDIA/Library/RedfishPlatformTaskLib/RedfishPlatformTaskLib.inf
  RedfishProtocolFeaturesLib|RedfishClientPkg/Library/RedfishProtocolFeaturesLib/RedfishProtocolFeaturesLib.inf
!endif

  FwVariableLib|Silicon/NVIDIA/Library/FwVariableLib/FwVariableLib.inf

  MemoryVerificationLib|Silicon/NVIDIA/Library/MemoryVerificationLib/MemoryVerificationLib.inf

  SystemContextLib|Silicon/NVIDIA/Library/SystemContextLib/SystemContextLib.inf
  StatusRegLib|Silicon/NVIDIA/Library/StatusRegLib/StatusRegLib.inf

  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf

  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf

!ifdef CONFIG_CONFIG_MANAGER
  DynamicPlatRepoLib|DynamicTablesPkg/Library/Common/DynamicPlatRepoLib/DynamicPlatRepoLib.inf
  HwInfoParserLib|DynamicTablesPkg/Library/FdtHwInfoParserLib/FdtHwInfoParserLib.inf
  MpCoreInfoLib|Silicon/NVIDIA/Library/MpCoreInfoLib/MpCoreInfoLib.inf
!endif

!if $(BUILD_PROJECT_TYPE) == PROJECT_MU
  BaseBinSecurityLib|MdePkg/Library/BaseBinSecurityLibNull/BaseBinSecurityLibNull.inf
  MemoryProtectionLib|MdeModulePkg/Library/BaseMemoryProtectionLibNull/BaseMemoryProtectionLibNull.inf
  PcdDatabaseLoaderLib|MdeModulePkg/Library/PcdDatabaseLoaderLib/Dxe/PcdDatabaseLoaderLibDxe.inf
  FltUsedLib|MdePkg/Library/FltUsedLib/FltUsedLib.inf
  SecurityLockAuditLib|MdeModulePkg/Library/SecurityLockAuditDebugMessageLib/SecurityLockAuditDebugMessageLib.inf
  CapsulePersistLib|MdeModulePkg/Library/CapsulePersistLibNull/CapsulePersistLibNull.inf
  ResetUtilityLib|MdeModulePkg/Library/ResetUtilityLib/ResetUtilityLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  MemoryTypeInformationChangeLib|MdeModulePkg/Library/MemoryTypeInformationChangeLibNull/MemoryTypeInformationChangeLibNull.inf
  HwResetSystemLib|ArmPkg/Library/ArmSmcPsciResetSystemLib/ArmSmcPsciResetSystemLib.inf
  MemoryTypeInfoSecVarCheckLib|MdeModulePkg/Library/MemoryTypeInfoSecVarCheckLib/MemoryTypeInfoSecVarCheckLib.inf
!endif

!ifdef CONFIG_REAL_TIME_CLOCK_NUVOTON
  RealTimeClockLib|Silicon/NVIDIA/Library/NuvotonRealTimeClockLib/NuvotonRealTimeClockLib.inf
!endif
!ifdef CONFIG_REAL_TIME_CLOCK_MAXIM
  RealTimeClockLib|Silicon/NVIDIA/Library/MaximRealTimeClockLib/MaximRealTimeClockLib.inf
!endif

!ifdef CONFIG_SOC_DATACENTER
  SsdtPcieSupportLib|Silicon/NVIDIA/Server/TH500/Library/SsdtPcieSupportLib/SsdtPcieSupportLib.inf
!else
  SsdtPcieSupportLib|DynamicTablesPkg/Library/Common/SsdtPcieSupportLib/SsdtPcieSupportLib.inf
!endif

!ifdef CONFIG_SECURITY_TPM_SUPPORT
  Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibTcg2/Tpm2DeviceLibTcg2.inf
  Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
  Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf
!endif
!ifdef CONFIG_SECURITY_TPM_DISCRETE
  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLib/DxeTcg2PhysicalPresenceLib.inf
  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLib/PeiDxeTpmPlatformHierarchyLib.inf
!endif
!ifdef CONFIG_SECURITY_TPM_FIRMWARE
  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLibNull/DxeTcg2PhysicalPresenceLibNull.inf
  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLibNull/PeiDxeTpmPlatformHierarchyLib.inf
!endif
!ifdef CONFIG_SECURITY_TPM_NONE
  Tcg2PhysicalPresenceLib|Silicon/NVIDIA/Library/DxeTcg2PhysicalPresenceLibNull/DxeTcg2PhysicalPresenceLibNull.inf
  TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLibNull/PeiDxeTpmPlatformHierarchyLib.inf
!endif

!ifdef CONFIG_SECURITY_UEFI_MENU_USER_AUTH
  # User Authentication for BIOS Setup Menu
  PlatformPasswordLib|Silicon/NVIDIA/Library/PlatformPasswordLib/PlatformPasswordLib.inf
!endif

[LibraryClasses.common.SEC]
  DebugAgentLib|ArmPkg/Library/DebugAgentSymbolsBaseLib/DebugAgentSymbolsBaseLib.inf
  DefaultExceptionHandlerLib|ArmPkg/Library/DefaultExceptionHandlerLib/DefaultExceptionHandlerLib.inf
  DebugLogScratchRegLib|Silicon/NVIDIA/Library/DebugLogScratchRegLib/DebugLogScratchRegLib.inf
!ifdef CONFIG_DEBUG_LOG_SCRATCH_REG
  DefaultExceptionCallbackLib|Silicon/NVIDIA/Server/TH500/Library/DefaultExceptionCallbackLibServer/DefaultExceptionCallbackLibServerPrePi.inf
!else
  DefaultExceptionCallbackLib|ArmPkg/Library/DefaultExceptionCallbackLibNull/DefaultExceptionCallbackLibNull.inf
!endif
  PrePiLib|EmbeddedPkg/Library/PrePiLib/PrePiLib.inf
  ExtractGuidedSectionLib|EmbeddedPkg/Library/PrePiExtractGuidedSectionLib/PrePiExtractGuidedSectionLib.inf
  LzmaDecompressLib|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
  MemoryAllocationLib|EmbeddedPkg/Library/PrePiMemoryAllocationLib/PrePiMemoryAllocationLib.inf
  HobLib|EmbeddedPkg/Library/PrePiHobLib/PrePiHobLib.inf
  PerformanceLib|MdeModulePkg/Library/PeiPerformanceLib/PeiPerformanceLib.inf
  ArmGicArchLib|ArmPkg/Library/ArmGicArchSecLib/ArmGicArchSecLib.inf
  SortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf

[LibraryClasses.common.PEI_CORE]
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  PeiCoreEntryPoint|MdePkg/Library/PeiCoreEntryPoint/PeiCoreEntryPoint.inf
  PerformanceLib|MdeModulePkg/Library/PeiPerformanceLib/PeiPerformanceLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/PeiReportStatusCodeLib/PeiReportStatusCodeLib.inf
  OemHookStatusCodeLib|MdeModulePkg/Library/OemHookStatusCodeLibNull/OemHookStatusCodeLibNull.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/PeiExtractGuidedSectionLib/PeiExtractGuidedSectionLib.inf

  PeiServicesTablePointerLib|ArmPkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf

[LibraryClasses.common.PEIM]
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  PerformanceLib|MdeModulePkg/Library/PeiPerformanceLib/PeiPerformanceLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/PeiReportStatusCodeLib/PeiReportStatusCodeLib.inf
  OemHookStatusCodeLib|MdeModulePkg/Library/OemHookStatusCodeLibNull/OemHookStatusCodeLibNull.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PeiResourcePublicationLib|MdePkg/Library/PeiResourcePublicationLib/PeiResourcePublicationLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/PeiExtractGuidedSectionLib/PeiExtractGuidedSectionLib.inf

  PeiServicesTablePointerLib|ArmPkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf

[LibraryClasses.common.SEC, LibraryClasses.common.PEIM]
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryInitPeiLib|ArmPlatformPkg/MemoryInitPei/MemoryInitPeiLib.inf

[LibraryClasses.common.DXE_CORE]
  HobLib|MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  MemoryAllocationLib|MdeModulePkg/Library/DxeCoreMemoryAllocationLib/DxeCoreMemoryAllocationLib.inf
  DxeCoreEntryPoint|MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  PerformanceLib|MdeModulePkg/Library/DxeCorePerformanceLib/DxeCorePerformanceLib.inf

[LibraryClasses.common.DXE_DRIVER]
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  AslTemplateTablesLib|Silicon/NVIDIA/Library/ASLTemplateTablesLib/ASLTemplateTablesLib.inf

[LibraryClasses.common.UEFI_APPLICATION]
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf

  # UiApp dependencies
  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf

!if $(TARGET) != RELEASE
  UnitTestResultReportLib|UnitTestFrameworkPkg/Library/UnitTestResultReportLib/UnitTestResultReportLibConOut.inf
!endif

[LibraryClasses.common.UEFI_DRIVER]
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibFmp/DxeRuntimeCapsuleLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/RuntimeDxeReportStatusCodeLib/RuntimeDxeReportStatusCodeLib.inf
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/RuntimeCryptLib.inf
  VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLibRuntimeDxe.inf
  DebugLib|MdePkg/Library/DxeRuntimeDebugLibSerialPort/DxeRuntimeDebugLibSerialPort.inf
!ifdef CONFIG_RUNTIME_FIRMWARE_CAPSULE_SUPPORTED
!ifdef CONFIG_FIRMWARE_MANAGEMENT_BLOB
  SavedCapsuleLib|Silicon/NVIDIA/Library/SavedCapsuleDataFlashLib/SavedCapsuleDataFlashLib.inf
!endif
!endif

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  NonDiscoverableDeviceRegistrationLib|MdeModulePkg/Library/NonDiscoverableDeviceRegistrationLib/NonDiscoverableDeviceRegistrationLib.inf
  PciSegmentLib|Silicon/NVIDIA/Library/PciSegmentLibPciRootBridgeConfigurationIo/PciSegmentLibPciRootBridgeConfigurationIo.inf
  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  DisplayUpdateProgressLib|MdeModulePkg/Library/DisplayUpdateProgressLibText/DisplayUpdateProgressLibText.inf
  AndroidBootImgLib|EmbeddedPkg/Library/AndroidBootImgLib/AndroidBootImgLib.inf
  AndroidBcbLib|Silicon/NVIDIA/Library/AndroidBcbLib/AndroidBcbLib.inf

[LibraryClasses.ARM, LibraryClasses.AARCH64]
  #
  # It is not possible to prevent the ARM compiler for generic intrinsic functions.
  # This library provides the instrinsic functions generate by a given compiler.
  # [LibraryClasses.ARM] and NULL mean link this library into all ARM images.
  #
  NULL|ArmPkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
  NULL|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf

  # Add support for GCC stack protector
  NULL|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf


[BuildOptions]
  *_*_*_CC_FLAGS  = -DDISABLE_NEW_DEPRECATED_INTERFACES

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFeatureFlag.common]
  gEfiMdePkgTokenSpaceGuid.PcdComponentNameDisable|FALSE
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnosticsDisable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdComponentName2Disable|FALSE
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnostics2Disable|TRUE

  # Use the Vector Table location in CpuDxe. We will not copy the Vector Table at PcdCpuVectorBaseAddress
  gArmTokenSpaceGuid.PcdRelocateVectorTable|FALSE

  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|TRUE

  gEfiMdeModulePkgTokenSpaceGuid.PcdTurnOffUsbLegacySupport|TRUE

  ## Indicates if S3 performance data will be supported in ACPI FPDT table.
  #   TRUE  - S3 performance data will be supported in ACPI FPDT table.
  #   FALSE - S3 performance data will not be supported in ACPI FPDT table.
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwarePerformanceDataTableS3Support|FALSE

  ## If TRUE, Graphics Output Protocol will be installed on virtual handle created by ConsplitterDxe.
  #  It could be set FALSE to save size.
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutGopSupport|TRUE

  gEfiMdeModulePkgTokenSpaceGuid.PcdTurnOffUsbLegacySupport|TRUE

  gEfiMdeModulePkgTokenSpaceGuid.PcdInstallAcpiSdtProtocol|TRUE

  gEfiMdeModulePkgTokenSpaceGuid.PcdVariableCollectStatistics|TRUE

!if $(BUILD_PROJECT_TYPE) == PROJECT_MU
  gEfiMdeModulePkgTokenSpaceGuid.PcdRequireIommu|FALSE
!endif
  gEfiMdeModulePkgTokenSpaceGuid.PcdEnableVariableRuntimeCache|FALSE

!ifndef CONFIG_SINGLE_BOOT_NONE
  gNVIDIATokenSpaceGuid.PcdSingleBootSupport|TRUE
!endif
  gEmbeddedTokenSpaceGuid.PcdAndroidBootLoadFile2|TRUE

!ifdef CONFIG_REDFISH
  gNVIDIATokenSpaceGuid.PcdRedfishSupported|TRUE
!endif
!ifdef CONFIG_IPMI_BMC
  gIpmiFeaturePkgTokenSpaceGuid.PcdIpmiFeatureEnable|True
!endif

!ifdef CONFIG_QUICK_BOOT
  gNVIDIATokenSpaceGuid.PcdQuickBootSupported|FALSE
!endif

!ifdef CONFIG_DEBUG_LEVEL_MENU_SUPPORT
  gNVIDIATokenSpaceGuid.PcdDebugMenuSupport|TRUE
!endif

!ifdef CONFIG_SECURITY_MM_FFA
  gArmTokenSpaceGuid.PcdFfaEnable|TRUE
!endif

!ifdef CONFIG_MEMORY_TEST
  #Memory test supported
  gNVIDIATokenSpaceGuid.PcdMemoryTestsSupported|TRUE
!endif

[PcdsFixedAtBuild.common]
!ifdef CONFIG_ACPI_THERMAL_ZONE_SINGLE
  gNVIDIATokenSpaceGuid.PcdUseSinglePassiveThermalZone|TRUE
!endif

  gNVIDIATokenSpaceGuid.PcdPlatformFamilyName|L"$(PLATFORM_NAME)"
  gNVIDIATokenSpaceGuid.PcdUefiVersionString|L"$(BUILDID_STRING)"
  gNVIDIATokenSpaceGuid.PcdUefiDateTimeBuiltString|L"$(BUILD_DATE_TIME)"
  gNVIDIATokenSpaceGuid.PcdBuildEpoch|$(BUILD_EPOCH)
  gNVIDIATokenSpaceGuid.PcdUefiHexVersionNumber|L"$(BUILD_HEX_VERSION)"

  gEfiMdeModulePkgTokenSpaceGuid.PcdCpuStackGuard|TRUE
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x22000

  gEfiNetworkPkgTokenSpaceGuid.PcdAllowHttpConnections|TRUE

  gEfiMdePkgTokenSpaceGuid.PcdMaximumUnicodeStringLength|1000000
  gEfiMdePkgTokenSpaceGuid.PcdMaximumAsciiStringLength|1000000
  gEfiMdePkgTokenSpaceGuid.PcdMaximumLinkedListLength|1000000
  gEfiMdePkgTokenSpaceGuid.PcdSpinLockTimeout|10000000
  gEfiMdePkgTokenSpaceGuid.PcdDebugClearMemoryValue|0xAF
  gEfiMdePkgTokenSpaceGuid.PcdPerformanceLibraryPropertyMask|1
  gEfiMdePkgTokenSpaceGuid.PcdPostCodePropertyMask|0
  gEfiMdePkgTokenSpaceGuid.PcdUefiLibMaxPrintBufferSize|320

  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultReceiveFifoDepth|32

  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
  # ASSERT_RESET_ENABLED       0x40
!if $(TARGET) == RELEASE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|$(CONFIG_DEBUG_PROPERTY_RELEASE)
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|$(CONFIG_DEBUG_PROPERTY_DEBUG)
!endif
  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseSerial|FALSE

  #  BIT0 - Enable Progress Code.
  #  BIT1 - Enable Error Code.
  #  BIT2 - Enable Debug Code.
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x07

  #  DEBUG_INIT      0x00000001  // Initialization
  #  DEBUG_WARN      0x00000002  // Warnings
  #  DEBUG_LOAD      0x00000004  // Load events
  #  DEBUG_FS        0x00000008  // EFI File system
  #  DEBUG_POOL      0x00000010  // Alloc & Free (pool)
  #  DEBUG_PAGE      0x00000020  // Alloc & Free (page)
  #  DEBUG_INFO      0x00000040  // Informational debug messages
  #  DEBUG_DISPATCH  0x00000080  // PEI/DXE/SMM Dispatchers
  #  DEBUG_VARIABLE  0x00000100  // Variable
  #  DEBUG_BM        0x00000400  // Boot Manager
  #  DEBUG_BLKIO     0x00001000  // BlkIo Driver
  #  DEBUG_NET       0x00004000  // SNP Driver
  #  DEBUG_UNDI      0x00010000  // UNDI Driver
  #  DEBUG_LOADFILE  0x00020000  // LoadFile
  #  DEBUG_EVENT     0x00080000  // Event messages
  #  DEBUG_GCD       0x00100000  // Global Coherency Database changes
  #  DEBUG_CACHE     0x00200000  // Memory range cachability changes
  #  DEBUG_VERBOSE   0x00400000  // Detailed debug messages that may
  #                              // significantly impact boot performance
  #  DEBUG_ERROR     0x80000000  // Error
  #
  #  If this is changed the boot watchdog value should be reevaluated.
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x8000000F

  gNVIDIATokenSpaceGuid.PcdAssertResetTimeoutValue|5

  #
  # Optional feature to help prevent EFI memory map fragments
  # Turned on and off via: PcdPrePiProduceMemoryTypeInformationHob
  # Values are in EFI Pages (4K). DXE Core will make sure that
  # at least this much of each type of memory can be allocated
  # from a single memory range. This way you only end up with
  # maximum of two fragements for each type in the memory map
  # (the memory used, and the free memory that was prereserved
  # but not used).
  #
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIReclaimMemory|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIMemoryNVS|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiReservedMemoryType|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesData|4000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesCode|4000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesCode|4000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesData|50000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderCode|500
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderData|0

  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

  # use the TTY terminal type
  gEfiMdePkgTokenSpaceGuid.PcdDefaultTerminalType|4

  # override the default values from SecurityPkg to ensure images from all sources are verified in secure boot
  gEfiSecurityPkgTokenSpaceGuid.PcdOptionRomImageVerificationPolicy|0x04
  gEfiSecurityPkgTokenSpaceGuid.PcdFixedMediaImageVerificationPolicy|0x04
  gEfiSecurityPkgTokenSpaceGuid.PcdRemovableMediaImageVerificationPolicy|0x04

  # GUID of the UI app
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }

  #
  # Enable strict image permissions for all images. (This applies
  # only to images that were built with >= 4 KB section alignment.)
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageProtectionPolicy|0x3

  #
  # Enable NX memory protection for all non-code regions, including OEM and OS
  # reserved ones, with the exception of LoaderData regions, of which OS loaders
  # (i.e., GRUB) may assume that its contents are executable.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdDxeNxMemoryProtectionPolicy|0xC000000000007FD1

  #
  # Enable the non-executable DXE stack. (This gets set up by DxeIpl)
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdSetNxForStack|TRUE

  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

  #
  # TCUart - Serial Terminal
  #
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartRxMailbox|0x03C10000
  gNVIDIATokenSpaceGuid.PcdTegraCombinedUartTxMailbox|0x0C168000

  #
  # UART 16550 parameters
  #
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialClockRate|407347200

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterStride|4
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseHardwareFlowControl|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialFifoControl|0x27
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialExtendedTxFifoSize|1

  #
  # PL011 UART Parameters
  #
  gArmPlatformTokenSpaceGuid.PL011UartClkInHz|1843200
  gArmPlatformTokenSpaceGuid.PL011UartInteger|0
  gArmPlatformTokenSpaceGuid.PL011UartFractional|0
  gArmPlatformTokenSpaceGuid.PL011UartRegOffsetVariant|0

  gArmTokenSpaceGuid.PcdMmBufferSize|65536

  gEmbeddedTokenSpaceGuid.PcdTimerPeriod|1000

!ifdef CONFIG_BOOT_UI_MENU_DISABLED
  gEfiMdeModulePkgTokenSpaceGuid.PcdDriverHealthConfigureForm|{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
!endif

!ifdef CONFIG_BOOT_WATCHDOG_TIMEOUT
  gNVIDIATokenSpaceGuid.PcdBootWatchdogTime|$(CONFIG_BOOT_WATCHDOG_TIMEOUT)
!endif

!ifdef CONFIG_REDFISH
  #
  # Redfish support
  #
  # Use gNVIDIABeforeConsoleEventGuid to startup Redfish feature drivers.
  # { 0x974180a0, 0xa203, 0x43c8, { 0xb1, 0x12, 0x48, 0x70, 0xe1, 0xdb, 0xd6, 0x48 } }
  gEfiRedfishClientPkgTokenSpaceGuid.PcdEdkIIRedfishFeatureDriverStartupEventGuid|{0xA0, 0x80, 0x41, 0x97, 0x03, 0xA2, 0xC8, 0x43, 0xB1, 0x12, 0x48, 0x70, 0xE1, 0xDB, 0xD6, 0x48 }
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishRestExServiceAccessModeInBand|False
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishDiscoverAccessModeInBand|False
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishDisableBootstrapCredentialService|TRUE
  gEfiRedfishClientPkgTokenSpaceGuid.PcdDefaultRedfishVersion|L"/redfish/v1/"
  gEfiRedfishClientPkgTokenSpaceGuid.PcdHttpGetRetry|0x05
  gEfiRedfishClientPkgTokenSpaceGuid.PcdHttpRetryWaitInSecond|2
  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishPlatformConfigFeatureProperty|1
!endif

!ifdef CONFIG_SECURITY_TPM_DISCRETE
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmPlatformClass|1
!endif

  #
  # Disable test key detection
  #
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceTestKeySha256Digest|{0}

[PcdsFixedAtBuild]
  #
  # Variable Services PCDs.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000

  #
  # FMP PCDs.
  #
!ifdef CONFIG_FIRMWARE_MANAGEMENT
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceImageIdName|L"NVIDIA System Firmware"
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceBuildTimeLowestSupportedVersion|0x0
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceProgressColor|0x0000FF00   # Green=255
  !include BaseTools/Source/Python/Pkcs7Sign/TestRoot.cer.gFmpDevicePkgTokenSpaceGuid.PcdFmpDevicePkcs7CertBufferXdr.inc
!endif

  #
  # SMBIOS entry point version
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosVersion|0x0306
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosDocRev|0x0

  # SMBIOS Type0 Firmware BIOS characteristics.
  gArmTokenSpaceGuid.PcdBiosCharacteristics|{0}
  gArmTokenSpaceGuid.PcdBiosCharacteristics.PciIsSupported            | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.PlugAndPlayIsSupported    | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.BiosIsUpgradable          | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.BiosShadowingAllowed      | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.BootFromCdIsSupported     | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.SelectableBootIsSupported | TRUE
  gArmTokenSpaceGuid.PcdBiosCharacteristics.SerialIsSupported         | TRUE

  gArmTokenSpaceGuid.PcdBiosCharacteristicsExtension|0x0C01

  #
  # SMBIOS Type 4 Information.
  #
  gArmTokenSpaceGuid.PcdProcessorManufacturer|L"NVIDIA"

  #
  # SMBIOS Type13 Bios Language Information in RFC 4646 code.
  # Only support en-US language in our UEFI.
  #
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLang|"en-US"
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLangCodes|"en-US"

  #
  # ACPI PCDs
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemId|"NVIDIA"
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|1
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultCreatorId|0x4144564E
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultCreatorRevision|1
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiExposedTableVersions|0x20

  #
  # ARM VFP PCDs
  #
  gArmTokenSpaceGuid.PcdVFPEnabled|1

  # Use Segment numbers as PCI UID
  gEdkiiDynamicTablesPkgTokenSpaceGuid.PcdPciUseSegmentAsUid|FALSE

  #
  # Security PCDs
  #
  gEfiMdePkgTokenSpaceGuid.PcdCpuRngSupportedAlgorithm|{ GUID("e43176d7-b6e8-4827-b784-7ffdc4b68561") }

  #
  # Do not attempt to load at large specified address
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageLargeAddressLoad|FALSE

!ifdef CONFIG_MEMORY_TEST
  gNVIDIATokenSpaceGuid.PcdExpectedPeiMemoryUsage|0x06000000
!endif

!ifdef CONFIG_SINGLE_BOOT_DEADLOOP_APP
gEfiSecurityPkgTokenSpaceGuid.PcdSingleBootApplicationGuid|{ GUID("2ecc727c-9d50-4467-a5f1-768ca4e9c23e") }
!else
!ifdef CONFIG_SINGLE_BOOT_L4T_LAUNCHER
gEfiSecurityPkgTokenSpaceGuid.PcdSingleBootApplicationGuid|{ GUID("cbf481fb-b373-4e78-aa7e-8631858e0217") }
!endif
!endif

  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(BUILDID_STRING)"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareReleaseDateString|L"$(BUILD_DATE_TIME)"

  gNVIDIATokenSpaceGuid.PcdFramebufferBarIndex|1
  gNVIDIATokenSpaceGuid.PcdFtpmShmSize|4096
  gEmbeddedTokenSpaceGuid.PcdAndroidKernelCommandLineOverflow|512

!ifdef CONFIG_L4T
  gNVIDIATokenSpaceGuid.PcdL4TConfigurationSupport|TRUE
!endif

  # SBSA Watchdog - always override the ArmPkg.dec values
  gArmTokenSpaceGuid.PcdGenericWatchdogControlBase|$(CONFIG_ARM_WATCHDOG_CONTROL_BASE)
  gArmTokenSpaceGuid.PcdGenericWatchdogRefreshBase|$(CONFIG_ARM_WATCHDOG_REFRESH_BASE)
  gArmTokenSpaceGuid.PcdGenericWatchdogEl2IntrNum|$(CONFIG_ARM_WATCHDOG_INTERRUPT)

!ifdef CONFIG_SOC_JETSON
  gArmTokenSpaceGuid.PcdBaseBoardManufacturer|L"NVIDIA"
  gArmTokenSpaceGuid.PcdBaseBoardProductName|L"Jetson"
!endif

!ifdef CONFIG_DEBUG_LOG_SCRATCH_REG
  gNVIDIATokenSpaceGuid.PcdNvLogToScratchRegs|TRUE
!endif

[PcdsDynamicDefault]
  #
  # Enable emulated variable NV mode in variable driver.
  #
!ifdef CONFIG_VARIABLE_EMULATED
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
!else
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE
!endif

  #
  # NV Storage PCDs.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x0

  #
  # FTW PCDs
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareSize|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingBase64|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingSize|0x0

  #
  # Default OEM Table ID
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|0x0

  #
  # PCI Root Complex
  #
  gArmTokenSpaceGuid.PcdPciBusMin|0
  gArmTokenSpaceGuid.PcdPciBusMax|0

  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x00000000

  #
  # ARM Generic Interrupt Controller
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x00000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x00000000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x00000000

  # SMBIOS
  ## Type 2 Table (Board Information)
  gNVIDIATokenSpaceGuid.PcdBoardChassisLocation|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdBoardVersion|L"Unknown"
  ## Type 1 Table (System Information)
  gNVIDIATokenSpaceGuid.PcdSystemSerialNum|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdSystemManufacturer|L"NVIDIA"
  gNVIDIATokenSpaceGuid.PcdSystemSku|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdSystemFamilyType|L"Unknown"
  ## Type 3 Table (Chassis Information)
  gNVIDIATokenSpaceGuid.PcdType3Info|{ 0 }
  gNVIDIATokenSpaceGuid.PcdType3Info.BootupState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.PowerSupplyState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.ThermalState|ChassisStateUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.SecurityStatus|ChassisSecurityStatusUnknown
  gNVIDIATokenSpaceGuid.PcdType3Info.Height|1
  gNVIDIATokenSpaceGuid.PcdType3Info.NumberofPowerCords|1
  gNVIDIATokenSpaceGuid.PcdType3Info.Type|1
  gNVIDIATokenSpaceGuid.PcdChassisSku|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisManufacturer|L"NVIDIA"
  gNVIDIATokenSpaceGuid.PcdChassisSerialNumber|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisAssetTag|L"Unknown"
  gNVIDIATokenSpaceGuid.PcdChassisVersion|L"Unknown"
  ##Type 32 (Boot Information)
  gNVIDIATokenSpaceGuid.PcdType32Info|{ 0 }
  gNVIDIATokenSpaceGuid.PcdType32Info.BootStatus|BootInformationStatusNoError
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmInstanceGuid|{0x5a, 0xf2, 0x6b, 0x28, 0xc3, 0xc2, 0x8c, 0x40, 0xb3, 0xb4, 0x25, 0xe6, 0x75, 0x8b, 0x73, 0x17}
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2InitializationPolicy|0
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2HashMask|0x00000006
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2HashAlgorithmBitmap|0x00000006

# Console Resolution
!ifdef CONFIG_DEFAULT_GOP_MODE_MAX
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|0
!endif
!ifdef CONFIG_DEFAULT_GOP_MODE_1080p
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|1920
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|1080
!endif
!ifdef CONFIG_DEFAULT_GOP_MODE_800_600
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|800
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|600
!endif

!ifdef CONFIG_SECURITY_MM_FFA
  ## MM Communicate
  ## Used for FFA boots with part of the boot chain skipped
  gArmTokenSpaceGuid.PcdMmBufferBase|0x8B5FE000
  gArmTokenSpaceGuid.PcdMmBufferSize|0x20000
!endif

!ifdef CONFIG_SECURITY_PHYSICAL_PRESENCE
  #
  # Allow physical presence activities
  #
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|TRUE
!else
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|FALSE
!endif

!ifdef CONFIG_SECURITY_DISABLE_PHYSICAL_PRESENCE_POST_SECUREBOOT
  #
  # Disable physical presence post secureboot enablement
  #
  gNVIDIATokenSpaceGuid.PcdDisablePhysicalPresence|TRUE
!else
  gNVIDIATokenSpaceGuid.PcdDisablePhysicalPresence|FALSE
!endif

[PcdsDynamicHii.common.DEFAULT]
  # Global variables
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|$(CONFIG_BOOT_DEFAULT_TIMEOUT)
  # NVIDIA internal variables
  gNVIDIATokenSpaceGuid.PcdPcieResourceConfigNeeded|L"PCIeResourceConfig"|gNVIDIATokenSpaceGuid|0x0|0|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpiConfigNeeded|L"PCIeInOsConfig"|gNVIDIATokenSpaceGuid|0x0|0|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpi|L"EnablePcieInOS"|gNVIDIATokenSpaceGuid|0x0|0|BS,NV
  gNVIDIATokenSpaceGuid.PcdSerialTypeConfig|L"SerialTypeConfig"|gNVIDIATokenSpaceGuid|0x0|0xFF|BS
  gNVIDIATokenSpaceGuid.PcdSerialPortConfig|L"SerialPortConfig"|gNVIDIATokenSpaceGuid|0x0|0xFF|BS,NV
  gNVIDIATokenSpaceGuid.PcdRedfishHostInterface|L"RedfishHostInterface"|gNVIDIATokenSpaceGuid|0x0|0x01|BS,NV
  gNVIDIATokenSpaceGuid.PcdDoInitialConsoleRegistration|L"DoInitialConsoleRegistration"|gNVIDIAPublicVariableGuid|0x0|1|BS,NV
  # NVIDIA Public variables
!ifdef CONFIG_BOOT_QUICK_BOOT_ENABLED
  gNVIDIATokenSpaceGuid.PcdQuickBootEnabled|L"QuickBootEnabled"|gNVIDIAPublicVariableGuid|0x0|1|BS,NV
!else
  gNVIDIATokenSpaceGuid.PcdQuickBootEnabled|L"QuickBootEnabled"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV
!endif
  gNVIDIATokenSpaceGuid.PcdNewDeviceHierarchy|L"NewDeviceHierarchy"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdOsChainStatusA|L"RootfsStatusSlotA"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdOsChainStatusB|L"RootfsStatusSlotB"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdL4TDefaultBootMode|L"L4TDefaultBootMode"|gNVIDIAPublicVariableGuid|0x00|0xFF|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdIpmiNetworkBootMode|L"IpmiNetworkBootMode"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdAcpiTimerEnabled|L"AcpiTimerEnabled"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV
  gNVIDIATokenSpaceGuid.PcdUefiShellEnabled|L"UefiShellEnabled"|gNVIDIAPublicVariableGuid|0x0|0x01|BS,NV
  gNVIDIATokenSpaceGuid.PcdDgpuDtEfifbSupport|L"DgpuDtEfifbSupport"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdEnabledPcieNicTopology|L"EnabledPcieNicTopology"|gNVIDIAPublicVariableGuid|0x0|{ 0x0, 0xFF, 0xFF, 0xFF, 0xFF }|BS,NV
!ifdef CONFIG_SOC_DISPLAY_HANDOFF_MODE_NEVER
  gNVIDIATokenSpaceGuid.PcdSocDisplayHandoffMode|L"SocDisplayHandoffMode"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
!endif
!ifdef CONFIG_SOC_DISPLAY_HANDOFF_MODE_ALWAYS
  gNVIDIATokenSpaceGuid.PcdSocDisplayHandoffMode|L"SocDisplayHandoffMode"|gNVIDIAPublicVariableGuid|0x0|1|BS,NV,RT
!endif
!ifdef CONFIG_SOC_DISPLAY_HANDOFF_MODE_AUTO
  gNVIDIATokenSpaceGuid.PcdSocDisplayHandoffMode|L"SocDisplayHandoffMode"|gNVIDIAPublicVariableGuid|0x0|2|BS,NV,RT
!endif
  # Memory Test options
  gNVIDIATokenSpaceGuid.PcdMemoryTest|L"MemoryTestControl"|gNVIDIAPublicVariableGuid|0x0|{0}|BS,NV,RT
!ifdef CONFIG_MEMORY_TEST
  !ifdef CONFIG_MEMORY_TEST_LEVEL_IGNORE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.TestLevel|MEMORY_TEST_LEVEL_IGNORE
  !endif
  !ifdef CONFIG_MEMORY_TEST_LEVEL_QUICK
    gNVIDIATokenSpaceGuid.PcdMemoryTest.TestLevel|MEMORY_TEST_LEVEL_QUICK
  !endif
  !ifdef CONFIG_MEMORY_TEST_LEVEL_SPARSE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.TestLevel|MEMORY_TEST_LEVEL_SPARSE
  !endif
  !ifdef CONFIG_MEMORY_TEST_LEVEL_EXTENSIVE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.TestLevel|MEMORY_TEST_LEVEL_EXTENSIVE
  !endif
  !ifdef CONFIG_MEMORY_TEST_NEXT_BOOT
    gNVIDIATokenSpaceGuid.PcdMemoryTest.NextBoot|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_SINGLE_BOOT
    gNVIDIATokenSpaceGuid.PcdMemoryTest.SingleBoot|TRUE
  !endif
  gNVIDIATokenSpaceGuid.PcdMemoryTest.TestIterations|$(CONFIG_MEMORY_TEST_ITERATIONS)
  !ifdef CONFIG_MEMORY_TEST_WALKING_1_BIT
    gNVIDIATokenSpaceGuid.PcdMemoryTest.Walking1BitEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_ADDRESS_CHECK
    gNVIDIATokenSpaceGuid.PcdMemoryTest.AddressCheckEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_MOVING_INVERSIONS_01
    gNVIDIATokenSpaceGuid.PcdMemoryTest.MovingInversions01Enabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_MOVING_INVERSIONS_8BIT
    gNVIDIATokenSpaceGuid.PcdMemoryTest.MovingInversions8BitEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_MOVING_INVERSIONS_RANDOM
    gNVIDIATokenSpaceGuid.PcdMemoryTest.MovingInversionsRandomEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_BLOCK_MOVE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.BlockMoveEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_MOVING_INVERSIONS_64BIT
    gNVIDIATokenSpaceGuid.PcdMemoryTest.MovingInversions64BitEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_RANDOM_SEQUENCE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.RandomNumberSequenceEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_MODULO_20
    gNVIDIATokenSpaceGuid.PcdMemoryTest.Modulo20RandomEnabled|TRUE
  !endif
  !ifdef CONFIG_MEMORY_TEST_BIT_FADE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.BitFadeEnabled|TRUE
    gNVIDIATokenSpaceGuid.PcdMemoryTest.BitFadePattern|$(CONFIG_MEMORY_TEST_BIT_FADE_PATTERN)
    gNVIDIATokenSpaceGuid.PcdMemoryTest.BitFadeWait|$(CONFIG_MEMORY_TEST_BIT_FADE_WAIT)
  !endif
!endif

  gNVIDIATokenSpaceGuid.PcdBoardRecoveryBoot|L"BoardRecoveryBoot"|gNVIDIAPublicVariableGuid|0x00|0x00|BS,NV,RT

!ifdef CONFIG_SERVER_POWER_CONTROLLERS
  gNVIDIATokenSpaceGuid.PcdServerPowerControlSetting|L"ServerPowerControlSetting"|gNVIDIAPublicVariableGuid|0x00|0x00|BS,NV,RT
!endif

[PcdsPatchableInModule]
  gArmTokenSpaceGuid.PcdFvBaseAddress|0x0
!ifdef CONFIG_FIRMWARE_MANAGEMENT
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceImageTypeIdGuid|{GUID("$(SYSTEM_FMP_ESRT_GUID)")}
  !ifdef CONFIG_FIRMWARE_CAPSULE_SUPPORTED
    gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleOnDiskSupport|TRUE
    gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|TRUE
    gEfiMdeModulePkgTokenSpaceGuid.PcdMaxSizeNonPopulateCapsule|$(CONFIG_FIRMWARE_CAPSULE_SIZE)
  !endif
!endif

!ifdef CONFIG_SOC_XAVIER
[PcdsDynamicDefault.common.T194]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x03881000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x03882000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA194'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T194)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gNVIDIATokenSpaceGuid.PcdSdhciCoherentDMADisable|TRUE
  gNVIDIATokenSpaceGuid.PcdBPMPPCIeControllerEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdBrBctLogicalSlotSize|0x4000
  gNVIDIATokenSpaceGuid.PcdBrBctDataSize|0x1000
  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|FALSE

[PcdsDynamicHii.common.T194]
  gNVIDIATokenSpaceGuid.PcdPcieResourceConfigNeeded|L"PCIeResourceConfig"|gNVIDIATokenSpaceGuid|0x0|1|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpiConfigNeeded|L"PCIeInOsConfig"|gNVIDIATokenSpaceGuid|0x0|1|BS
!endif

!ifdef CONFIG_SOC_ORIN
[PcdsDynamicDefault.common.T234]
  gArmTokenSpaceGuid.PcdGicDistributorBase|0X0F400000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0X0F440000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TEGRA234'
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_T234)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gNVIDIATokenSpaceGuid.PcdSdhciCoherentDMADisable|TRUE
  gNVIDIATokenSpaceGuid.PcdBPMPPCIeControllerEnable|TRUE
  gNVIDIATokenSpaceGuid.PcdBrBctLogicalSlotSize|0x10000
  gNVIDIATokenSpaceGuid.PcdBrBctDataSize|0x2000
  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|TRUE
  gNVIDIATokenSpaceGuid.PcdGptIsPerBootChain|TRUE

[PcdsDynamicHii.common.T234SLT]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicHii.common.T234Presil]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicDefault.common.T234Presil]
  gNVIDIATokenSpaceGuid.PcdVirtualRTC|TRUE
  gNVIDIATokenSpaceGuid.PcdTegraStmmEnabled|FALSE
!endif

!ifdef CONFIG_SOC_GRACE
[PcdsDynamicDefault.common.TH500Presil]
  gEfiMdeModulePkgTokenSpaceGuid.PcdSrIovSupport|FALSE

[PcdsFixedAtBuild]
 !ifdef CONFIG_DEBUG_LOG_SCRATCH_REG
   ## SCRATCH_SCRATCH_<i> [i >= 21] is used to log crash related information.
   gNVIDIATokenSpaceGuid.PcdNvScratchRegBase|0x0000C3903DC
   ## Register 21(i = 21) is to log the FW image name.
   ## This is the starting offset of the debug data and must
   ## be used for FW image name.
   gNVIDIATokenSpaceGuid.PcdNvFwNameStartReg|21
   ## Register 21 to log file name.
   gNVIDIATokenSpaceGuid.PcdNvFileNameStartReg|22
   ## Restrict the File Name to 20 32-bit values.
   gNVIDIATokenSpaceGuid.PcdNvFileNameRegLimit|20
   ## Register 42 to log line number.
   gNVIDIATokenSpaceGuid.PcdNvLineNumStartReg|42
   ## Restrict the line number to 2 32-bit values.
   gNVIDIATokenSpaceGuid.PcdNvLineNumRegLimit|2
   ## Register 44 to log Exception type.
   gNVIDIATokenSpaceGuid.PcdExceptionTypeStartReg|44
   ## Register 45 to log FAR Reg.
   gNVIDIATokenSpaceGuid.PcdFARStartReg|45
   ## Register 47 to log FP.
   gNVIDIATokenSpaceGuid.PcdFPStartReg|47
   ## Total number of registers for FP (across frames).
   gNVIDIATokenSpaceGuid.PcdFPRegLimit|53
   ## Limit of registers for each Frame.
   gNVIDIATokenSpaceGuid.PcdPerFpLimit|7
 !endif

[PcdsDynamicDefault.common.TH500]
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|'TH500   '
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500)")}
  gNVIDIATokenSpaceGuid.PcdNonUniqueSystemFmpCapsuleImageTypeIdGuid|{GUID("$(FMP_SYSTEM_IMAGE_TYPE_ID_GUID_TH500_NVIDIA)")}
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0X80000000
  gArmTokenSpaceGuid.PcdPciBusMin|0
  gArmTokenSpaceGuid.PcdPciBusMax|1
  gNVIDIATokenSpaceGuid.PcdPciConfigurationSpaceBaseAddress|0x2DA00000
  # SMBIOS for TH500
  ## Type 0 Table (BIOS Information)
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L"NVIDIA"
  ## RTC
  gNVIDIATokenSpaceGuid.PcdCpuHasRtcControl|FALSE
  ## TPM
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2AcpiTableRev|4
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2PhysicalPresenceFlags|0x00002
  gEfiSecurityPkgTokenSpaceGuid.PcdRandomizePlatformHierarchy|TRUE
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2SelfTestPolicy|1
  gEfiSecurityPkgTokenSpaceGuid.PcdTcgLogAreaMinLen|0x20000
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2FinalLogAreaLen|0x10000
  gEfiMdeModulePkgTokenSpaceGuid.PcdTcgPfpMeasurementRevision|105

  gNVIDIATokenSpaceGuid.PcdAffinityMpIdrSupported|TRUE
  gNVIDIATokenSpaceGuid.PcdNonSecureQspiAvailable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(BUILDID_STRING)"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareReleaseDateString|L"$(BUILD_DATE_TIME)"

[PcdsDynamicHii.common.TH500]
  gNVIDIATokenSpaceGuid.PcdEnabledPcieNicTopology|L"EnabledPcieNicTopology"|gNVIDIAPublicVariableGuid|0x0|{ 0x0, 0x0, 0x0, 0x0, 0x0 }|BS,NV

[PcdsDynamicHii.common.TH500Presil]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|2

[PcdsDynamicDefault.common.TH500Presil]
  gNVIDIATokenSpaceGuid.PcdVirtualRTC|TRUE
!endif

[Components.common]
  #
  # PEI Phase modules
  #
  Silicon/NVIDIA/PrePi/PrePi.inf {
    <LibraryClasses>
      DefaultExceptionHandlerLib|Silicon/NVIDIA/Library/PrePiDefaultExceptionHandlerLib/PrePiDefaultExceptionHandlerLib.inf
  }

  #
  # DXE
  #
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      NULL|MdeModulePkg/Library/DxeCrc32GuidedSectionExtractLib/DxeCrc32GuidedSectionExtractLib.inf
  }

!ifdef CONFIG_ACPI
  #
  # Aml Generation
  #
  Silicon/NVIDIA/Drivers/AmlGenerationDxe/AmlGenerationDxe.inf

  #
  # Aml Patching
  #
  Silicon/NVIDIA/Drivers/AmlPatchDxe/AmlPatchDxe.inf

  #
  # ACPI Support
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf {
    <LibraryClasses>
!ifdef CONFIG_DEVICETREE
      NULL|EmbeddedPkg/Library/PlatformHasAcpiLib/PlatformHasAcpiLib.inf
!endif
  }
  Silicon/NVIDIA/Drivers/AcpiDtbSsdtGenerator/AcpiDtbSsdtGenerator.inf
!endif

  #
  # FDT Support
  #
!ifdef CONFIG_DEVICETREE
  !ifdef CONFIG_ACPI
  EmbeddedPkg/Drivers/DtPlatformDxe/DtPlatformDxe.inf {
    <LibraryClasses>
      DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbKernelLoaderLib.inf
  }
  !else
  Silicon/NVIDIA/Drivers/DtOnlyPlatformDxe/DtOnlyPlatformDxe.inf {
    <LibraryClasses>
      DtPlatformDtbLoaderLib|Silicon/NVIDIA/Library/DxeDtPlatformDtbLoaderLib/DxeDtPlatformDtbKernelLoaderLib.inf
  }
  !endif
!endif

  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  }

  #
  # Architectural Protocols
  #
  ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf {
     <LibraryClasses>
!ifdef CONFIG_SECURITY_UEFI_MENU_USER_AUTH
       NULL|Silicon/NVIDIA/Library/UserAuthenticationLib/UserAuthenticationLib.inf
!endif
!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
       NULL|SecurityPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.inf
!endif
!ifdef CONFIG_SECURITY_TPM_SUPPORT
       NULL|SecurityPkg/Library/DxeTpm2MeasureBootLib/DxeTpm2MeasureBootLib.inf
!endif
  }
  MdeModulePkg/Universal/SectionExtractionDxe/SectionExtractionDxe.inf
  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  MdeModulePkg/Universal/Metronome/Metronome.inf
  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  MdeModulePkg/Universal/TimestampDxe/TimestampDxe.inf
  MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf
  Silicon/NVIDIA/Drivers/DebugStatusCodeDxe/DebugStatusCodeDxe.inf {
    <LibraryClasses>
      DebugLib|Silicon/NVIDIA/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  }

  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  MdeModulePkg/Universal/PrintDxe/PrintDxe.inf

  Silicon/NVIDIA/Drivers/DefaultVariableDxe/DefaultVariableDxe.inf
!ifdef CONFIG_VARIABLE_NON_SECURE_SUPPORTED
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf {
    <LibraryClasses>
!ifndef CONFIG_VARIABLE_EMULATED
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
      NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
!endif
  }
!endif
!ifdef CONFIG_SECURITY_MM_SUPPORT
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableSmmRuntimeDxe.inf
!endif

  #
  # Tegra Platform Configuration
  #
  Silicon/NVIDIA/Drivers/TegraPlatformInit/TegraPlatformInitDxe.inf

  #
  # Platform Configuration Manager Data
  #
!ifdef CONFIG_CONFIG_MANAGER
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      # Note: DataRepoLib must be first!
      ConfigurationManagerDataRepoLib|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ConfigurationManagerDataRepoLib.inf
      PlatformAslTablesLib|Silicon/NVIDIA/Drivers/ConfigurationManagerData/AcpiTableList/PlatformASLTablesLib/PlatformASLTablesLib.inf
      AcpiTableListParser|Silicon/NVIDIA/Drivers/ConfigurationManagerData/AcpiTableList/AcpiTableList.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/BootArchInfo/BootArchInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/FixedFeatureFlags/FixedFeatureFlags.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/GenericTimer/GenericTimer.inf
!ifdef CONFIG_SOC_GRACE
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/GenericWatchdog/GenericWatchdog.inf
!endif
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/PowerManagementProfile/PowerManagementProfile.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SerialPortInfo/SerialPortInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ProcHierarchyInfo/ProcHierarchyInfo.inf
!ifdef CONFIG_SOC_GRACE
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/EthernetInfo/EthernetInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/DsdtPatcher/DsdtPatcher.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/TelemetryInfo/TelemetryInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/TpmInfo/TpmInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ThermalZoneInfo/ThermalZoneInfo.inf
!endif
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ProtocolBasedObjects/ProtocolBasedObjects.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/AhciInfo/AhciInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SdhciInfo/SdhciInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/I2cInfo/I2cInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/HdaInfo/HdaInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/FanInfo/FanInfo.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/Gic/Gic.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/Iort/Iort.inf

!ifdef CONFIG_SOC_GRACE
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ApmtParser/ApmtParser.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/Tpm2Parser/Tpm2Parser.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/MpamParser/MpamParser.inf

      HbmParserLib|Silicon/NVIDIA/Drivers/ConfigurationManagerData/HbmParserLib/HbmParserLib.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SlitParser/SlitParser.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SratParser/SratParser.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/HmatParser/HmatParser.inf

      IpmiParser|Silicon/NVIDIA/Drivers/ConfigurationManagerData/IpmiParser/IpmiParser.inf
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SpmiParser/SpmiParser.inf
!endif

!ifdef CONFIG_SECURITY_MM_SUPPORT
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/WsmtParser/WsmtParser.inf
!endif
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/Dbg2NetworkParser/Dbg2NetworkParser.inf

!ifdef CONFIG_SOC_GRACE
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SmbiosParser/SmbiosParser.inf
!endif

      # Note: SsdtTableGenerator must be last!
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/SsdtTableGenerator/SsdtTableGenerator.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -I$(BIN_DIR)/Silicon/NVIDIA/Drivers/ConfigurationManagerData/AcpiTableList/PlatformASLTablesLib/PlatformASLTablesLib/OUTPUT -I$(BIN_DIR)/Silicon/NVIDIA/Library/ASLTemplateTablesLib/ASLTemplateTablesLib/OUTPUT
  }
!endif

  #
  # BPMP-FW IPC protocol
  #
  Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf
  Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  Silicon/NVIDIA/Drivers/I2cExpanderGpio/I2cExpanderGpio.inf

  #
  # I2C Support
  #
  MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf

!ifdef CONFIG_IPMI_BMC
  Silicon/NVIDIA/Drivers/I2cIoBmcSsifDxe/I2cIoBmcSsifDxe.inf
  Silicon/NVIDIA/Drivers/ArmSbmrStatusCodeDxe/ArmSbmrStatusCodeDxe.inf
  Silicon/NVIDIA/Drivers/OemDescStatusCodeDxe/OemDescStatusCodeDxe.inf
  IpmiFeaturePkg/Frb/FrbDxe.inf

  #
  # IPMI Null Driver
  #
  Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Ipmi Blob Transfer support
  #
  Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/IpmiBlobTransferDxe.inf
  Silicon/NVIDIA/Drivers/SmbiosBmcTransfer/SmbiosBmcTransfer.inf
!endif

  #
  # Logo support
  #
!ifdef CONFIG_LOGO
  Silicon/NVIDIA/Drivers/Logo/LogoDxe.inf
!ifdef CONFIG_ACPI
  MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
!endif
!endif

  #
  # SMBIOS/DMI
  #
!ifdef CONFIG_SMBIOS
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
!endif
!ifdef CONFIG_SMBIOS_ARM
  ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  Silicon/NVIDIA/Drivers/SmbiosOemDxe/SmbiosMiscOemDxe/SmbiosMiscOemDxe.inf
  ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
!endif

  #
  # Bds
  #
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf {
    <LibraryClasses>
!ifdef CONFIG_BOOT_NVIDIA_CUSTOM_OPTION_DESCRIPTION
      NULL|Silicon/NVIDIA/Library/PlatformBootManagerBootDescriptionLib/PlatformBootManagerBootDescriptionLib.inf
!endif
  }
!ifndef CONFIG_BOOT_UI_MENU_DISABLED
  MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/DeviceManagerUiLib/DeviceManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootManagerUiLib/BootManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootMaintenanceManagerUiLib/BootMaintenanceManagerUiLib.inf
!ifdef CONFIG_BOOT_NVIDIA_CUSTOM_OPTION_DESCRIPTION
      NULL|Silicon/NVIDIA/Library/PlatformBootManagerBootDescriptionLib/PlatformBootManagerBootDescriptionLib.inf
!endif
      NULL|Silicon/NVIDIA/Library/UiMenuStartupLib/UiMenuStartupLib.inf
  }
  MdeModulePkg/Universal/DriverHealthManagerDxe/DriverHealthManagerDxe.inf
!endif

  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
  ArmPkg/Drivers/TimerDxe/TimerDxe.inf
!ifdef CONFIG_ARM_WATCHDOG
  ArmPkg/Drivers/GenericWatchdogDxe/GenericWatchdogDxe.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/PlatformIsSiliconLib/PlatformIsSiliconLib.inf
  }
  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/PlatformIsPresiliconLib/PlatformIsPresiliconLib.inf
  }
!else
  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
!endif
  Silicon/NVIDIA/Drivers/BootWatchdog/BootWatchdog.inf

  #
  # Firmware Performance Data Table (FPDT)
  #
  MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTableDxe/FirmwarePerformanceDxe.inf {
    <LibraryClasses>
      LockBoxLib|MdeModulePkg/Library/LockBoxNullLib/LockBoxNullLib.inf
  }

  #
  # Networking stack
  #
!ifdef CONFIG_NETWORKING
  NetworkPkg/DpcDxe/DpcDxe.inf
  NetworkPkg/ArpDxe/ArpDxe.inf
  NetworkPkg/MnpDxe/MnpDxe.inf
  NetworkPkg/SnpDxe/SnpDxe.inf
  NetworkPkg/TcpDxe/TcpDxe.inf
  NetworkPkg/DnsDxe/DnsDxe.inf
!ifdef CONFIG_NETWORKING_IPV4
  NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  NetworkPkg/Udp4Dxe/Udp4Dxe.inf
!endif
!ifdef CONFIG_NETWORKING_IPV6
  NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf
  NetworkPkg/Udp6Dxe/Udp6Dxe.inf
!endif
!ifdef CONFIG_NETWORKING_VLAN
  NetworkPkg/VlanConfigDxe/VlanConfigDxe.inf
!endif
!ifdef CONFIG_NETWORKING_PXE
  NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf
!endif
!ifdef CONFIG_NETWORKING_ISCSI
  NetworkPkg/IScsiDxe/IScsiDxe.inf {
    <LibraryClasses>
!ifdef CONFIG_CONFIG_MANAGER
    NULL|Silicon/NVIDIA/Library/PlatformHasConfigManagerLib/PlatformHasConfigManagerLib.inf
!endif
  }
!endif
!ifdef CONFIG_NETWORKING_HTTP
  NetworkPkg/TlsDxe/TlsDxe.inf
  NetworkPkg/TlsAuthConfigDxe/TlsAuthConfigDxe.inf
  NetworkPkg/HttpDxe/HttpDxe.inf
  NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
  NetworkPkg/HttpBootDxe/HttpBootDxe.inf
!endif
!endif

  #
  # NIC drivers
  #
!ifdef CONFIG_NETWORKING_DEVICE_NVIDIA
  Silicon/NVIDIA/Drivers/EqosDeviceDxe/EqosDeviceDxe.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_INTEL
!ifdef $(ENABLE_INTELUNDI)
  #
  # Intel 500 series NIC
  #
  IntelUndiPkg/XGigUndiDxe/XGigUndiDxe.inf {
    <LibraryClasses>
      StdLibC|IntelUndiPkg/LibC/LibC.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -Wno-unused-value -Wno-multistatement-macros
  }
  #
  # Intel 700 series NIC
  #
  IntelUndiPkg/I40eUndiDxe/I40eUndiDxe.inf {
    <LibraryClasses>
      StdLibC|IntelUndiPkg/LibC/LibC.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -Wno-unused -Wno-multistatement-macros -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype
  }
  #
  # Intel 200 series NIC
  #
  IntelUndiPkg/GigUndiDxe/GigUndiDxe.inf {
    <LibraryClasses>
      StdLibC|IntelUndiPkg/LibC/LibC.inf
    <BuildOptions>
      *_*_*_PLATFORM_FLAGS = -Wno-unused -Wno-incompatible-pointer-types -Wno-pointer-sign -Wno-misleading-indentation
  }
!endif
!endif
!ifdef CONFIG_NETWORKING_DEVICE_REALTEK
!ifdef CONFIG_USB
  Drivers/Realtek/Bus/Usb/UsbNetworking/RtkUsbUndiDxe.inf
!endif
  Drivers/Realtek/Bus/Pcie/PcieNetworking/RtkUndiDxe.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_ASIX
  Drivers/ASIX/Bus/Usb/UsbNetworking/Ax88772c/Ax88772c.inf
!endif
!ifdef CONFIG_NETWORKING_DEVICE_USB_RNDIS
  Silicon/NVIDIA/Drivers/BmcNic/UsbRndisDxe/UsbRndisDxe.inf
!endif

  # Filesystems
!ifdef CONFIG_FS_FIRMWARE_VOLUME
  MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf
!endif
!ifdef CONFIG_FS_FAT
  FatPkg/EnhancedFatDxe/Fat.inf
!endif

  #
  # GPT/MBR partitioning
  #
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf

  #
  # Serial console driver
  #
!ifdef CONFIG_SERIAL_PORT_CONSOLE_MDEMODULE
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf
!endif
!ifdef CONFIG_SERIAL_PORT_CONSOLE_TEGRA
  Silicon/NVIDIA/Drivers/TegraUartDxe/TegraUartDxe.inf
!endif

  #
  # PCI Support
  #
!ifdef CONFIG_PCIE
  MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf
  ArmPkg/Drivers/ArmPciCpuIo2Dxe/ArmPciCpuIo2Dxe.inf
!endif
!ifdef CONFIG_DESIGNWARE_PCIE
  Silicon/NVIDIA/Drivers/TegraP2UDxe/TegraP2UDxe.inf
  Silicon/NVIDIA/Drivers/PcieDWControllerDxe/PcieControllerDxe.inf
!endif
!ifdef CONFIG_NVIDIA_PCIE
  Silicon/NVIDIA/Drivers/PcieControllerDxe/PcieControllerDxe.inf
!endif

  #
  # NVMe boot devices
  #
!ifdef CONFIG_NVME
  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf
!endif

  #
  # EMMC/SD
  #
!ifdef CONFIG_NVIDIA_SDHCI
  Silicon/NVIDIA/Drivers/SdMmcControllerDxe/SdMmcControllerDxe.inf
!endif
!ifdef CONFIG_SDCARD
  MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
!endif
!ifdef CONFIG_EMMC
  MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
!endif
!ifdef CONFIG_SDHCI
  MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
!endif

  #
  # UFS support
  #
!ifdef CONFIG_UFS
  Silicon/NVIDIA/Drivers/UfsControllerDxe/UfsControllerDxe.inf
  MdeModulePkg/Bus/Pci/UfsPciHcDxe/UfsPciHcDxe.inf
  MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
!endif

  #
  # SATA Controller
  #
!ifdef CONFIG_SATA
  MdeModulePkg/Bus/Ata/AtaBusDxe/AtaBusDxe.inf
  MdeModulePkg/Bus/Ata/AtaAtapiPassThru/AtaAtapiPassThru.inf
!endif

  #
  # SCSI support (Used for UFS, SATA, iSCSI)
  #
!ifdef CONFIG_SCSI
  MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  MdeModulePkg/Bus/Pci/SataControllerDxe/SataControllerDxe.inf
!endif

  #
  # Usb Support
  #
!ifdef CONFIG_SOC_USB
  Silicon/NVIDIA/Drivers/UsbFirmwareDxe/UsbFirmwareDxe.inf
  Silicon/NVIDIA/Drivers/UsbPadCtlDxe/UsbPadCtlDxe.inf
  Silicon/NVIDIA/Drivers/XhciControllerDxe/XhciControllerDxe.inf
  Silicon/NVIDIA/Drivers/XusbControllerDxe/XusbControllerDxe.inf
!endif
!ifdef CONFIG_UHCI
  MdeModulePkg/Bus/Pci/UhciDxe/UhciDxe.inf
!endif
!ifdef CONFIG_EHCI
  MdeModulePkg/Bus/Pci/EhciDxe/EhciDxe.inf
!endif
!ifdef CONFIG_XHCI
  MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
!endif
!ifdef CONFIG_USB
  MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
!endif
!ifdef CONFIG_USB_KEYBOARD
  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
!endif
!ifdef CONFIG_USB_MOUSE
  MdeModulePkg/Bus/Usb/UsbMouseDxe/UsbMouseDxe.inf
  MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf
!endif
!ifdef CONFIG_USB_MASS_STORAGE
  MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
!endif
!ifdef CONFIG_USB_FW_RENESAS
!ifdef $(ENABLE_RENESAS_USB_FW)
  #
  # Renesas USB controller firmware upload support
  #
  Silicon/NVIDIA/Drivers/RenesasUsbCtlr/RenesasUsbCtlr.inf
!endif
!endif

  #
  # Generic non-discoverable pcie used for many soc devices
  #
  MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf

  #
  # Display hardware
  #
!ifdef CONFIG_NVIDIA_DISPLAY
  Silicon/NVIDIA/Drivers/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  Silicon/NVIDIA/Drivers/NvDisplayControllerDxe/NvDisplayControllerDxe.inf
  !ifdef CONFIG_SOC_XAVIER
    Silicon/NVIDIA/Tegra/T194/Drivers/T194GraphicsOutputDxe/T194GraphicsOutputDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
    }
  !endif
!endif

  #
  # SE RNG
  #
!ifdef CONFIG_SOC_XAVIER
  Silicon/NVIDIA/Tegra/T194/Drivers/SeRngDxe/SeRngDxe.inf
!endif
!ifdef CONFIG_SOC_ORIN
  Silicon/NVIDIA/Tegra/T234/Drivers/SeRngDxe/SeRngDxe.inf
!endif

  #
  # Common Jetson devices
  #
!ifdef CONFIG_SOC_JETSON
  #
  # PinMux Support
  #
  Silicon/NVIDIA/Drivers/PinMuxDxe/PinMuxDxe.inf

  #
  # Tegra PWM clock driver
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
  # BasicProfiler Support
  #
  Silicon/NVIDIA/Drivers/BasicProfilerDxe/BasicProfilerDxe.inf

  #
  # Default Variables Device Tree overlay builds
  #
  Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree.inf
!endif

!ifdef CONFIG_RAS
  #
  # APEI
  #
  Silicon/NVIDIA/Server/TH500/Drivers/ApeiDxe/Apei.inf {
    <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
!endif

!ifdef CONFIG_BOOTLOADER_RAMDISK
  #
  # Ramloaded OS
  #
  Silicon/NVIDIA/Drivers/RamDiskOS/RamDiskOS.inf
!endif

!ifdef CONFIG_SOC_GRACE
  #
  # GH100 GPU Driver
  #
  Silicon/NVIDIA/Server/TH500/Drivers/TH500GpuDxe/TH500GpuDxe.inf

  #
  # Oem Partition Sample Driver.
  #
  Silicon/NVIDIA/Server/TH500/Drivers/OemPartitionDxeMm/OemPartitionDxe.inf

  #
  # FPGA I2C Support
  #
  Silicon/NVIDIA/Drivers/FpgaI2c/FpgaI2c.inf
!endif

  #
  # UEFI application (Shell Embedded Boot Loader)
  #
!ifdef CONFIG_SHELL
  ShellPkg/Application/Shell/Shell.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
    <LibraryClasses>
      ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
!ifdef CONFIG_SHELL_LEVEL_1_SUPPORTED
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellInstall1CommandsLib/UefiShellInstall1CommandsLib.inf
!endif
!ifdef CONFIG_SHELL_LEVEL_2_SUPPORTED
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
!endif
!ifdef CONFIG_SHELL_LEVEL_3_SUPPORTED
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
!endif
!ifdef CONFIG_SHELL_DEBUG_COMMANDS
      NULL|ShellPkg/Library/UefiShellDebug1CommandsLib/UefiShellDebug1CommandsLib.inf
!endif
!ifdef CONFIG_SHELL_DRIVER_COMMANDS
      NULL|ShellPkg/Library/UefiShellDriver1CommandsLib/UefiShellDriver1CommandsLib.inf
!endif
!ifdef CONFIG_ACPI
      NULL|ShellPkg/Library/UefiShellAcpiViewCommandLib/UefiShellAcpiViewCommandLib.inf
!endif
!ifdef CONFIG_SHELL_NETWORK_COMMANDS
      NULL|ShellPkg/Library/UefiShellNetwork1CommandsLib/UefiShellNetwork1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellNetwork2CommandsLib/UefiShellNetwork2CommandsLib.inf
!endif
      BcfgCommandLib|ShellPkg/Library/UefiShellBcfgCommandLib/UefiShellBcfgCommandLib.inf
  }
!endif
!ifdef CONFIG_SHELL_NETWORK_COMMANDS
!ifdef CONFIG_NETWORKING_HTTP
  ShellPkg/DynamicCommand/HttpDynamicCommand/HttpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
!endif
  ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
!endif
  MdeModulePkg/Application/VariableInfo/VariableInfo.inf
  Silicon/NVIDIA/Application/HiiKeywordUtil/HiiKeywordUtil.inf

!ifdef CONFIG_FIRMWARE_CAPSULE_SUPPORTED
  #
  # Capsule Application
  #
  MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf
!endif
!ifdef CONFIG_RUNTIME_FIRMWARE_CAPSULE_SUPPORTED
  Silicon/NVIDIA/Drivers/NvidiaCapsuleRuntimeDxe/NvidiaCapsuleRuntimeDxe.inf
!endif

  #
  # Test Applications
  #
!ifdef CONFIG_NVIDIA_SHELL_APPLICATIONS
  Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

!ifdef CONFIG_IPMI_BMC
  Silicon/NVIDIA/Application/IpmiUtility/IpmiUtility.inf
!endif
!ifdef CONFIG_REDFISH
  #
  # Host Interface test Application
  #
  Silicon/NVIDIA/Application/HostInterfaceUtil/HostInterfaceUtil.inf  {
    <LibraryClasses>
      RedfishPlatformCredentialLib|Silicon/NVIDIA/Library/RedfishPlatformCredentialLib/RedfishPlatformCredentialLib.inf
  }

!endif

!if $(TARGET) != RELEASE
  Silicon/NVIDIA/Test/ShellTest/BootOrderTest/BootOrderTestUefiShell.inf
  Silicon/NVIDIA/Test/ShellTest/EfiGopTest/EfiGopTestUefiShell.inf
  Silicon/NVIDIA/Test/ShellTest/UefiMemMapTest/UefiMemMapTestUefiShell.inf
!endif

  Silicon/NVIDIA/Application/RegulatorUtil/RegulatorUtil.inf
  Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf
!ifdef CONFIG_SOC_USB
  Silicon/NVIDIA/Application/FalconUtil/FalconUtil.inf
!endif
!endif

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  #
  # Default Security Key Enrollment Application.
  #
  SecurityPkg/EnrollFromDefaultKeysApp/EnrollFromDefaultKeysApp.inf
!endif

!ifdef CONFIG_FIRMWARE_PARTITION_SUPPORT
  #
  # FW Partition and FW Image
  #
  Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe/FwPartitionNorFlashDxe.inf
  Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe/FwPartitionBlockIoDxe.inf
  Silicon/NVIDIA/Drivers/FwImageDxe/FwImageDxe.inf
  Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf
!endif

!ifdef CONFIG_FIRMWARE_MANAGEMENT
  #
  # Esrt - EFI System Resource Table
  #
  MdeModulePkg/Universal/EsrtFmpDxe/EsrtFmpDxe.inf

  #
  # Fmp
  #
  FmpDevicePkg/FmpDxe/FmpDxe.inf {
    <Defines>
      #
      # FILE_GUID is used as ESRT GUID
      #
      FILE_GUID = $(SYSTEM_FMP_ESRT_GUID)
  }
!endif
!ifdef CONFIG_FIRMWARE_MANAGEMENT_EROT
  #
  # NS ERoT
  #
  Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiDxe.inf

  #
  # MM MCTP
  #
  Silicon/NVIDIA/Drivers/MctpMmDxe/MctpMmDxe.inf
!endif
!ifdef CONFIG_FIRMWARE_MANAGEMENT_BLOB
  #
  # FW Partition
  #
  Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionMmDxe.inf
!endif

!ifdef CONFIG_CONFIG_MANAGER
  #
  # Dynamic Table Factory Dxe
  #
  DynamicTablesPkg/Drivers/DynamicTableFactoryDxe/DynamicTableFactoryDxe.inf {
    <LibraryClasses>
!ifdef CONFIG_ACPI
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiDbg2LibArm/AcpiDbg2LibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiFadtLibArm/AcpiFadtLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiGtdtLibArm/AcpiGtdtLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiIortLibArm/AcpiIortLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiMadtLibArm/AcpiMadtLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiMcfgLibArm/AcpiMcfgLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiMpamLibArm/AcpiMpamLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiPpttLibArm/AcpiPpttLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiRawLibArm/AcpiRawLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSpcrLibArm/AcpiSpcrLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSratLibArm/AcpiSratLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSpmiLibArm/AcpiSpmiLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiTpm2LibArm/AcpiTpm2LibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtCpuTopologyLibArm/SsdtCpuTopologyLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtPcieLibArm/SsdtPcieLibArm.inf
      # AML Fixup
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtSerialPortLibArm/SsdtSerialPortLibArm.inf
!endif

!ifdef CONFIG_SMBIOS
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType1Lib/SmbiosType1Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType0Lib/SmbiosType0Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType2Lib/SmbiosType2Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType3Lib/SmbiosType3Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType8Lib/SmbiosType8Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType9Lib/SmbiosType9Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType11Lib/SmbiosType11Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType13Lib/SmbiosType13Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType14Lib/SmbiosType14Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType32Lib/SmbiosType32Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType38Lib/SmbiosType38Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType39Lib/SmbiosType39Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType41Lib/SmbiosType41Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType43Lib/SmbiosType43Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/Standard/SmbiosType45Lib/SmbiosType45Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/SmbiosType16Lib/SmbiosType16Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/SmbiosType17Lib/SmbiosType17Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/SmbiosType19Lib/SmbiosType19Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/SmbiosType4Lib/SmbiosType4Lib.inf
      NULL|DynamicTablesPkg/Library/Smbios/SmbiosType7Lib/SmbiosType7Lib.inf
!endif
  }

  #
  # Dynamic Tables Manager Dxe
  #
  DynamicTablesPkg/Drivers/DynamicTableManagerDxe/DynamicTableManagerDxe.inf
  #
  # Configuration Manager
  #
  Silicon/NVIDIA/Drivers/ConfigurationManagerTokenDxe/ConfigurationManagerTokenDxe.inf
  Silicon/NVIDIA/Drivers/ConfigurationManager/ConfigurationManagerDxe.inf
!endif

  #
  # Tegra Platform Boot Manager
  #
  Silicon/NVIDIA/Drivers/TegraPlatformBootManager/TegraPlatformBootManagerDxe.inf

  #
  # HTTP boot support requires RamDiskDxe
  MdeModulePkg/Universal/Disk/RamDiskDxe/RamDiskDxe.inf

  #
  # Custom HII Support
  #
  Silicon/NVIDIA/Drivers/NvidiaConfigDxe/NvidiaConfigDxe.inf

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  #
  # Secure Boot Support
  #
  SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf
!endif

  #
  # Security drivers
  #
  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf
!endif

!ifdef CONFIG_RCM_BOOT
  #
  # RCM Support
  #
  Silicon/NVIDIA/Drivers/RcmDxe/RcmDxe.inf
!endif

  #
  # Platform To Driver Configuration Support
  #
  Silicon/NVIDIA/Drivers/PlatformToDriverConfiguration/PlatformToDriverConfiguration.inf

  #
  # Regulator support
  #
  Silicon/NVIDIA/Drivers/RegulatorDxe/RegulatorDxe.inf

  #
  # Capsule Update Policy Protocol
  #
  Silicon/NVIDIA/Drivers/CapsuleUpdatePolicyDxe/CapsuleUpdatePolicyDxe.inf

  #
  # QSPI Support
  #
  Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerDxe.inf

  #
  # NOR Flash Support
  #
  Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashDxe.inf

!ifdef CONFIG_VARIABLE_NON_SECURE_SUPPORTED
!ifndef CONFIG_VARIABLE_EMULATED
  #
  # FVB Support
  #
  Silicon/NVIDIA/Drivers/FvbDxe/FvbDxe.inf
  Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashDxe.inf
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
  }
!endif
!endif

  #
  # Tegra EEPROM Support
  #
  Silicon/NVIDIA/Drivers/EepromDxe/Eeprom.inf

  #
  # Tegra Pin Control support
  #
  Silicon/NVIDIA/Drivers/TegraPinControlDxe/TegraPinControlDxe.inf

  #
  # Boot Chain Protocol
  #
  Silicon/NVIDIA/Drivers/BootChainDxe/BootChainDxe.inf

  #
  # CPU Frequency support
  #
  Silicon/NVIDIA/Drivers/TegraCpuFreqDxe/TegraCpuFreqDxe.inf

  #
  # Tegra Controller Enable Driver
  #
  Silicon/NVIDIA/Drivers/TegraControllerEnableDxe/TegraControllerEnableDxe.inf

  #
  # Uphy Configuration Driver
  #
  Silicon/NVIDIA/Drivers/UphyConfigurationDxe/UphyConfigurationDxe.inf

  #
  # Fuse Support
  #
  Silicon/NVIDIA/Drivers/EFuseDxe/EFuseDxe.inf

!ifdef CONFIG_SECURITY_UEFI_SECURE_BOOT
  #
  # Secure Boot Provision
  #
  Silicon/NVIDIA/Drivers/NvSecureBootProvisionDxe/NvSecureBootProvisionDxe.inf
!endif

  #
  # Memory Tests
  #
!ifdef CONFIG_MEMORY_TEST
  Silicon/NVIDIA/Drivers/GenericMpMemoryTestDxe/GenericMemoryTestDxe.inf
!endif

  #
  # Multiprocessor support
  #
  ArmPkg/Drivers/ArmPsciMpServicesDxe/ArmPsciMpServicesDxe.inf
  Silicon/NVIDIA/Drivers/ThreadingDxe/ThreadingDxe.inf

  #
  # L4T launcher
  #
  Silicon/NVIDIA/Application/L4TLauncher/L4TLauncher.inf {
    <LibraryClasses>
      DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  }

  # WFI App
!ifdef CONFIG_SINGLE_BOOT_DEADLOOP_APP
  Silicon/NVIDIA/Application/WfiApp/WfiApp.inf
!endif

  # Boot support for mkbootimg partitions
!ifdef CONFIG_BOOT_NATIVE_ANDROID_PARTITION_SUPPORT
  Silicon/NVIDIA/Drivers/AndroidBootDxe/AndroidBootDxe.inf
!endif

  #
  # Android Fastboot
  #
!ifdef CONFIG_BOOT_ANDROID_FASTBOOT
  Silicon/NVIDIA/Drivers/TegraUsbDeviceControllerDxe/TegraUsbDeviceControllerDxe.inf
  EmbeddedPkg/Application/AndroidFastboot/AndroidFastbootApp.inf
  EmbeddedPkg/Drivers/AndroidFastbootTransportUsbDxe/FastbootTransportUsbDxe.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/TegraUsbNullLib/TegraUsbNullLib.inf
  }
  Silicon/NVIDIA/Drivers/TegraFastBootDxe/TegraFastBootDxe.inf
!endif

  #
  # Gpio Power Off Driver
  #
  Silicon/NVIDIA/Drivers/GpioPowerOffDxe/GpioPowerOffDxe.inf

!ifdef CONFIG_REDFISH
#
  # Redfish Host Interface Support
  #
  RedfishPkg/RedfishHostInterfaceDxe/RedfishHostInterfaceDxe.inf {
    <LibraryClasses>
      RedfishPlatformHostInterfaceLib|Silicon/NVIDIA/Library/RedfishPlatformHostInterfaceOemLib/RedfishPlatformHostInterfaceOemLib.inf
  }
  Silicon/NVIDIA/Drivers/RedfishHostInterfaceControlDxe/RedfishHostInterfaceControlDxe.inf {
    <LibraryClasses>
      RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  }

  #
  # Default TLS Cipher Suites for Redfish HTTPS support
  #
  Silicon/NVIDIA/Drivers/RedfishCipherDxe/RedfishCipherDxe.inf {
    <LibraryClasses>
      RedfishEventLib|RedfishClientPkg/Library/RedfishEventLib/RedfishEventLib.inf
  }

  #
  # Redfish support
  #
  RedfishPkg/RestJsonStructureDxe/RestJsonStructureDxe.inf
  RedfishPkg/RedfishRestExDxe/RedfishRestExDxe.inf
  RedfishPkg/RedfishCredentialDxe/RedfishCredentialDxe.inf
  RedfishPkg/RedfishDiscoverDxe/RedfishDiscoverDxe.inf
  RedfishPkg/RedfishConfigHandler/RedfishConfigHandlerDriver.inf
  RedfishPkg/RedfishPlatformConfigDxe/RedfishPlatformConfigDxe.inf
  MdeModulePkg/Universal/RegularExpressionDxe/RegularExpressionDxe.inf

  RedfishClientPkg/RedfishFeatureCoreDxe/RedfishFeatureCoreDxe.inf
  RedfishClientPkg/RedfishETagDxe/RedfishETagDxe.inf
  RedfishClientPkg/RedfishConfigLangMapDxe/RedfishConfigLangMapDxe.inf
  RedfishClientPkg/RedfishHttpDxe/RedfishHttpDxe.inf
  RedfishClientPkg/Features/ComputerSystemCollectionDxe/ComputerSystemCollectionDxe.inf
  RedfishClientPkg/Features/ComputerSystem/v1_17_0/Dxe/ComputerSystemDxe.inf {
    <LibraryClasses>
      RedfishResourceIdentifyLib|Silicon/NVIDIA/Library/RedfishResourceIdentifyLibComputerSystem/v1_17_0/RedfishResourceIdentifyLibComputerSystem.inf
  }
  RedfishClientPkg/Features/ComputerSystem/v1_22_0/Dxe/ComputerSystemDxe.inf {
    <LibraryClasses>
      RedfishResourceIdentifyLib|Silicon/NVIDIA/Library/RedfishResourceIdentifyLibComputerSystem/v1_22_0/RedfishResourceIdentifyLibComputerSystem.inf
  }
  RedfishClientPkg/Features/Bios/v1_2_0/Dxe/BiosDxe.inf
  RedfishClientPkg/Features/SecureBoot/v1_1_0/Dxe/SecureBootDxe.inf
  RedfishClientPkg/Converter/ComputerSystem/v1_17_0/RedfishComputerSystem_V1_17_0_Dxe.inf
  RedfishClientPkg/Converter/ComputerSystem/v1_22_0/RedfishComputerSystem_V1_22_0_Dxe.inf
  RedfishClientPkg/Converter/ComputerSystemCollection/RedfishComputerSystemCollection_Dxe.inf
  RedfishClientPkg/Features/BootOptionCollection/BootOptionCollectionDxe.inf
  RedfishClientPkg/Features/BootOption/v1_0_4/Dxe/BootOptionDxe.inf
  RedfishClientPkg/Converter/Bios/v1_2_0/RedfishBios_V1_2_0_Dxe.inf
  RedfishClientPkg/Converter/BootOptionCollection/RedfishBootOptionCollection_Dxe.inf
  RedfishClientPkg/Converter/BootOption/v1_0_4/RedfishBootOption_V1_0_4_Dxe.inf
  RedfishClientPkg/Converter/SecureBoot/v1_1_0/RedfishSecureBoot_V1_1_0_Dxe.inf
  RedfishClientPkg/RedfishBootstrapAccountDxe/RedfishBootstrapAccountDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishBiosDxe/PlatformRedfishBiosDxe.inf
  Silicon/NVIDIA/Drivers/RedfishFirmwareInfoDxe/RedfishFirmwareInfoDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishBootDxe/PlatformRedfishBootDxe.inf
  Silicon/NVIDIA/Drivers/PlatformRedfishOverrideDxe/PlatformRedfishOverrideDxe.inf
  RedfishClientPkg/Features/SecureBootDatabaseCollectionDxe/SecureBootDatabaseCollectionDxe.inf
  RedfishClientPkg/Converter/SecureBootDatabaseCollection/RedfishSecureBootDatabaseCollection_Dxe.inf
  RedfishClientPkg/Features/SecureBootDatabase/v1_0_1/Dxe/SecureBootDatabaseDxe.inf
  RedfishClientPkg/Converter/SecureBootDatabase/v1_0_1/RedfishSecureBootDatabase_V1_0_1_Dxe.inf
  RedfishClientPkg/RedfishTaskServiceDxe/RedfishTaskServiceDxe.inf
  RedfishClientPkg/RedfishSecureBootKeysDxe/RedfishSecureBootKeysDxe.inf
!endif

  #
  # MM Communication
  #
!ifdef CONFIG_SECURITY_MM_FFA
  Silicon/NVIDIA/Drivers/MmCommunicateFfaDxe/MmCommunicationFfa.inf {
    <LibraryClasses>
      NULL|StandaloneMmPkg/Library/VariableMmDependency/VariableMmDependency.inf
      NULL|Silicon/NVIDIA/Library/StandaloneMmUnavailableLib/StandaloneMmUnavailableLib.inf
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
!endif
!ifdef CONFIG_SECURITY_MM_OPTEE
  Silicon/NVIDIA/Drivers/MmCommunicationOpteeDxe/MmCommunicationOptee.inf {
    <LibraryClasses>
      NULL|StandaloneMmPkg/Library/VariableMmDependency/VariableMmDependency.inf
      NULL|Silicon/NVIDIA/Library/StandaloneMmUnavailableLib/StandaloneMmUnavailableLib.inf
  }
!endif

  #
  # TPM
  #
!ifdef CONFIG_SECURITY_TPM_SUPPORT
!ifdef CONFIG_SECURITY_TPM_DISCRETE
  Silicon/NVIDIA/Drivers/Tpm2Dxe/Tpm2Dxe.inf
  MdeModulePkg/Universal/SmbiosMeasurementDxe/SmbiosMeasurementDxe.inf
!endif
  SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf {
    <LibraryClasses>
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha384/HashInstanceLibSha384.inf
!ifdef CONFIG_SECURITY_TPM_DISCRETE
      Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibRouter/Tpm2DeviceLibRouterDxe.inf
      NULL|Silicon/NVIDIA/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
!endif
!ifdef CONFIG_SECURITY_TPM_FIRMWARE
      Tpm2DeviceLib|Silicon/NVIDIA/Library/OpteeTpmDeviceLib/OpteeTpmDeviceLib.inf
!endif
  }
  Silicon/NVIDIA/Drivers/Tcg2Config/Tcg2ConfigDxe.inf
!endif

!ifdef CONFIG_SECURITY_UEFI_MENU_USER_AUTH
  Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationDxe.inf
!endif

  #
  # Server Power Control Driver
  #
!ifdef CONFIG_SERVER_POWER_CONTROLLERS
  Silicon/NVIDIA/Drivers/ServerPowerControlDxe/ServerPowerControlDxe.inf
!endif

[Components.AARCH64]
  #
  # EBC
  #
!ifdef CONFIG_EBC
  MdeModulePkg/Universal/EbcDxe/EbcDxe.inf
!endif
