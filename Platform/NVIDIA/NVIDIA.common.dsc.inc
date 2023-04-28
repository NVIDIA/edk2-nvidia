#
#  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  Copyright (c) 2011-2018, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent

[BuildOptions.common.EDKII.DXE_CORE,BuildOptions.common.EDKII.DXE_DRIVER,BuildOptions.common.EDKII.UEFI_DRIVER,BuildOptions.common.EDKII.UEFI_APPLICATION]
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x1000

[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  GCC:*_*_ARM_DLINK_FLAGS = -z common-page-size=0x1000
  GCC:*_*_AARCH64_DLINK_FLAGS = -z common-page-size=0x10000

[BuildOptions.common]
  GCC:*_*_*_PLATFORM_FLAGS = -fstack-protector-strong

[BuildOptions]
  *_*_*_ASL_FLAGS = -tc -li -so -oi
  GCC:*_*_*_CC_FLAGS = -D ENABLE_MD5_DEPRECATED_INTERFACES

[LibraryClasses.common]
  DebugLib|Silicon/NVIDIA/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

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
  PlatformBootOrderLib|Silicon/NVIDIA/Library/PlatformBootOrderLib/PlatformBootOrderLib.inf
  BootLogoLib|MdeModulePkg/Library/BootLogoLib/BootLogoLib.inf

  AcpiLib|EmbeddedPkg/Library/AcpiLib/AcpiLib.inf

  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

  #
  # Secure Boot dependencies
  #
  IntrinsicLib|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  OpensslLib|CryptoPkg/Library/OpensslLib/OpensslLibFull.inf
  TpmMeasurementLib|SecurityPkg/Library/DxeTpmMeasurementLib/DxeTpmMeasurementLib.inf
  AuthVariableLib|SecurityPkg/Library/AuthVariableLib/AuthVariableLib.inf
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/BaseCryptLib.inf
  RngLib|MdePkg/Library/BaseRngLibTimerLib/BaseRngLibTimerLib.inf

  PlatformSecureLib|SecurityPkg/Library/PlatformSecureLibNull/PlatformSecureLibNull.inf
  VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
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

  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
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

  AmlLib|DynamicTablesPkg/Library/Common/AmlLib/AmlLib.inf
  SsdtSerialPortFixupLib|Silicon/NVIDIA/Library/SsdtSerialPortFixupLib/SsdtSerialPortFixupLib.inf
  TableHelperLib|DynamicTablesPkg/Library/Common/TableHelperLib/TableHelperLib.inf
  AcpiHelperLib|DynamicTablesPkg/Library/Common/AcpiHelperLib/AcpiHelperLib.inf
  SmbiosStringTableLib|DynamicTablesPkg/Library/Common/SmbiosStringTableLib/SmbiosStringTableLib.inf

  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  PciHostBridgeLib|Silicon/NVIDIA/Library/PciHostBridgeLib/PciHostBridgeLib.inf

  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf

  PL011UartLib|ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf

  MceAriLib|Silicon/NVIDIA/Library/MceAriLib/MceAriLib.inf
  NvgLib|Silicon/NVIDIA/Library/NvgLib/NvgLib.inf
  FloorSweepingLib|Silicon/NVIDIA/Library/FloorSweepingLib/FloorSweepingLib.inf
  DeviceTreeHelperLib|Silicon/NVIDIA/Library/DeviceTreeHelperLib/DeviceTreeHelperLib.inf
  DisplayDeviceTreeHelperLib|Silicon/NVIDIA/Library/DisplayDeviceTreeHelperLib/DisplayDeviceTreeHelperLib.inf

  Crc8Lib|Silicon/NVIDIA/Library/Crc8Lib/Crc8Lib.inf

  IpmiBaseLib|IpmiFeaturePkg/Library/IpmiBaseLib/IpmiBaseLib.inf
  IpmiCommandLib|IpmiFeaturePkg/Library/IpmiCommandLib/IpmiCommandLib.inf
  MctpBaseLib|Silicon/NVIDIA/Library/MctpBaseLib/MctpBaseLib.inf
  MctpNvVdmLib|Silicon/NVIDIA/Library/MctpNvVdmLib/MctpNvVdmLib.inf
  ErotLib|Silicon/NVIDIA/Library/ErotLib/ErotLib.inf

  FwVariableLib|Silicon/NVIDIA/Library/FwVariableLib/FwVariableLib.inf

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

[LibraryClasses.common.SEC]
  DebugAgentLib|ArmPkg/Library/DebugAgentSymbolsBaseLib/DebugAgentSymbolsBaseLib.inf
  DefaultExceptionHandlerLib|ArmPkg/Library/DefaultExceptionHandlerLib/DefaultExceptionHandlerLib.inf
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

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  NonDiscoverableDeviceRegistrationLib|MdeModulePkg/Library/NonDiscoverableDeviceRegistrationLib/NonDiscoverableDeviceRegistrationLib.inf
  PciSegmentLib|Silicon/NVIDIA/Library/PciSegmentLibPciRootBridgeConfigurationIo/PciSegmentLibPciRootBridgeConfigurationIo.inf
  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  DisplayUpdateProgressLib|MdeModulePkg/Library/DisplayUpdateProgressLibText/DisplayUpdateProgressLibText.inf

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
  gEfiMdePkgTokenSpaceGuid.PcdComponentNameDisable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnosticsDisable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdComponentName2Disable|TRUE
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

[PcdsFixedAtBuild.common]
  gNVIDIATokenSpaceGuid.PcdUefiVersionString|L"$(BUILDID_STRING)"
  gNVIDIATokenSpaceGuid.PcdUefiDateTimeBuiltString|L"$(BUILD_DATE_TIME)"

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

  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
  # ASSERT_RESET_ENABLED       0x40
!if $(TARGET) == RELEASE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x41
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x4f
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
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesData|80
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesCode|65
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesCode|400
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesData|20000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderCode|20
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderData|0

  #
  # FileSystem
  #

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

[PcdsFixedAtBuild]
  #
  # Variable Services PCDs.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000

  #
  # FMP PCDs.
  #
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceImageIdName|L"NVIDIA System Firmware"
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceBuildTimeLowestSupportedVersion|0x0
  gFmpDevicePkgTokenSpaceGuid.PcdFmpDeviceProgressColor|0x0000FF00   # Green=255
  !include BaseTools/Source/Python/Pkcs7Sign/TestRoot.cer.gFmpDevicePkgTokenSpaceGuid.PcdFmpDevicePkcs7CertBufferXdr.inc

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
  gEdkiiDynamicTablesPkgTokenSpaceGuid.PcdPciUseSegmentAsUid|TRUE

  #
  # Security PCDs
  #
  gEfiSecurityPkgTokenSpaceGuid.PcdCpuRngSupportedAlgorithm|{0xe4,0x31,0x76,0xd7,0xb6,0xe8,0x48,0x27,0xb7,0x84,0x7f,0xfd,0xc4,0xb6,0x85,0x61}

  #
  # Do not attempt to load at large specified address
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageLargeAddressLoad|FALSE

[PcdsDynamicDefault]
  #
  # Enable emulated variable NV mode in variable driver.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE

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

[PcdsDynamicDefault.common.DEFAULT]
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x00000000

  #
  # ARM Generic Interrupt Controller
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x00000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x00000000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x00000000

  #
  # Allow physical presence activities
  #
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|FALSE

[PcdsDynamicHii.common.DEFAULT]
  # Global variables
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|5
  # NVIDIA internal variables
  gNVIDIATokenSpaceGuid.PcdPcieResourceConfigNeeded|L"PCIeResourceConfig"|gNVIDIATokenSpaceGuid|0x0|0|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpiConfigNeeded|L"PCIeInOsConfig"|gNVIDIATokenSpaceGuid|0x0|0|BS
  gNVIDIATokenSpaceGuid.PcdPcieEntryInAcpi|L"EnablePcieInOS"|gNVIDIATokenSpaceGuid|0x0|0|BS,NV
  gNVIDIATokenSpaceGuid.PcdSerialTypeConfig|L"SerialTypeConfig"|gNVIDIATokenSpaceGuid|0x0|0xFF|BS
  gNVIDIATokenSpaceGuid.PcdSerialPortConfig|L"SerialPortConfig"|gNVIDIATokenSpaceGuid|0x0|0xFF|BS,NV
  gNVIDIATokenSpaceGuid.PcdRedfishHostInterface|L"RedfishHostInterface"|gNVIDIATokenSpaceGuid|0x0|0x01|BS,NV
  gNVIDIATokenSpaceGuid.PcdDoInitialConsoleRegistration|L"DoInitialConsoleRegistration"|gNVIDIAPublicVariableGuid|0x0|1|BS,NV
  # NVIDIA Public variables
  gNVIDIATokenSpaceGuid.PcdQuickBootEnabled|L"QuickBootEnabled"|gNVIDIAPublicVariableGuid|0x0|1|BS,NV
  gNVIDIATokenSpaceGuid.PcdNewDeviceHierarchy|L"NewDeviceHierarchy"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV
  gNVIDIATokenSpaceGuid.PcdOsChainStatusA|L"RootfsStatusSlotA"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdOsChainStatusB|L"RootfsStatusSlotB"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdL4TDefaultBootMode|L"L4TDefaultBootMode"|gNVIDIAPublicVariableGuid|0x00|0xFF|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdIpmiNetworkBootMode|L"IpmiNetworkBootMode"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT
  gNVIDIATokenSpaceGuid.PcdAcpiTimerEnabled|L"AcpiTimerEnabled"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV
  gNVIDIATokenSpaceGuid.PcdUefiShellEnabled|L"UefiShellEnabled"|gNVIDIAPublicVariableGuid|0x0|0x01|BS,NV
  gNVIDIATokenSpaceGuid.PcdDgpuDtEfifbSupport|L"DgpuDtEfifbSupport"|gNVIDIAPublicVariableGuid|0x0|0|BS,NV,RT

[Components.common]
  #
  # PEI Phase modules
  #
  Silicon/NVIDIA/PrePi/PrePi.inf

  #
  # DXE
  #
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      NULL|MdeModulePkg/Library/DxeCrc32GuidedSectionExtractLib/DxeCrc32GuidedSectionExtractLib.inf
  }

  #
  # Aml Generation
  #
  Silicon/NVIDIA/Drivers/AmlGenerationDxe/AmlGenerationDxe.inf

  #
  # Aml Patching
  #
  Silicon/NVIDIA/Drivers/AmlPatchDxe/AmlPatchDxe.inf

  #
  # Device discovery protocol
  #
  Silicon/NVIDIA/Drivers/DeviceDiscovery/DeviceDiscoveryDxe.inf

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
       NULL|Silicon/NVIDIA/Library/UserAuthenticationLib/UserAuthenticationLib.inf
       NULL|SecurityPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.inf
       NULL|SecurityPkg/Library/DxeTpm2MeasureBootLib/DxeTpm2MeasureBootLib.inf
  }
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
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf {
    <LibraryClasses>
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
      NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableSmmRuntimeDxe.inf

  #
  # BPMP-FW IPC protocol
  #
  Silicon/NVIDIA/Drivers/BpmpIpc/BpmpIpcDxe.inf
  Silicon/NVIDIA/Drivers/BpmpScmi/BpmpScmiDxe.inf
  Silicon/NVIDIA/Application/ClockUtil/ClockUtil.inf
  Silicon/NVIDIA/Drivers/BpmpI2c/BpmpI2cDxe.inf

  #
  # GPIO driver/utility
  #
  Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioDxe.inf
  Silicon/NVIDIA/Drivers/I2cExpanderGpio/I2cExpanderGpio.inf
  Silicon/NVIDIA/Application/GpioUtil/GpioUtil.inf

  #
  # I2C Support
  #
  MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  Silicon/NVIDIA/Drivers/TegraI2c/TegraI2cDxe.inf
  Silicon/NVIDIA/Drivers/I2cIoBmcSsifDxe/I2cIoBmcSsifDxe.inf
  Silicon/NVIDIA/Drivers/ArmSbmrStatusCodeDxe/ArmSbmrStatusCodeDxe.inf
  IpmiFeaturePkg/Frb/FrbDxe.inf

  #
  # IPMI Null Driver
  #
  Silicon/NVIDIA/Drivers/IpmiNullDxe/IpmiNullDxe.inf

  #
  # Graphics console support
  #
  Silicon/NVIDIA/Drivers/Logo/LogoDxe.inf
  MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf

  #
  # SMBIOS/DMI
  #
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf

  #
  # Bds
  #
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf {
    <LibraryClasses>
      NULL|Silicon/NVIDIA/Library/PlatformBootManagerBootDescriptionLib/PlatformBootManagerBootDescriptionLib.inf
  }
  MdeModulePkg/Application/UiApp/UiApp.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/DeviceManagerUiLib/DeviceManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootManagerUiLib/BootManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootMaintenanceManagerUiLib/BootMaintenanceManagerUiLib.inf
      NULL|Silicon/NVIDIA/Library/PlatformBootManagerBootDescriptionLib/PlatformBootManagerBootDescriptionLib.inf
  }
  MdeModulePkg/Universal/DriverHealthManagerDxe/DriverHealthManagerDxe.inf

  #
  # Test Applications
  #
  Silicon/NVIDIA/Application/StackCheck/StackCheck.inf

!if $(TARGET) != RELEASE
  Silicon/NVIDIA/Test/ShellTest/BootOrderTest/BootOrderTestUefiShell.inf
  Silicon/NVIDIA/Test/ShellTest/EfiGopTest/EfiGopTestUefiShell.inf
  Silicon/NVIDIA/Test/ShellTest/UefiMemMapTest/UefiMemMapTestUefiShell.inf
!endif

  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  ArmPkg/Drivers/ArmGic/ArmGicDxe.inf
  ArmPkg/Drivers/TimerDxe/TimerDxe.inf
  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf

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
  NetworkPkg/DpcDxe/DpcDxe.inf
  NetworkPkg/ArpDxe/ArpDxe.inf
  NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  NetworkPkg/MnpDxe/MnpDxe.inf
  NetworkPkg/SnpDxe/SnpDxe.inf
  NetworkPkg/VlanConfigDxe/VlanConfigDxe.inf
  NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  NetworkPkg/TcpDxe/TcpDxe.inf
  NetworkPkg/Udp4Dxe/Udp4Dxe.inf
  NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf
  NetworkPkg/IScsiDxe/IScsiDxe.inf
  NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  NetworkPkg/Udp6Dxe/Udp6Dxe.inf
  NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf

  NetworkPkg/TlsDxe/TlsDxe.inf
  NetworkPkg/TlsAuthConfigDxe/TlsAuthConfigDxe.inf
  NetworkPkg/DnsDxe/DnsDxe.inf
  NetworkPkg/HttpDxe/HttpDxe.inf
  NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
  NetworkPkg/HttpBootDxe/HttpBootDxe.inf

  # FV Filesystems
  MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf
  MdeModulePkg/Universal/SectionExtractionDxe/SectionExtractionDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf

  #
  # GPT/MBR partitioning
  #
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf

  #
  # PCI Support
  #
  MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf

  #
  # SATA Controller
  #
  MdeModulePkg/Bus/Ata/AtaBusDxe/AtaBusDxe.inf
  MdeModulePkg/Bus/Ata/AtaAtapiPassThru/AtaAtapiPassThru.inf
  MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  MdeModulePkg/Bus/Pci/SataControllerDxe/SataControllerDxe.inf

  #
  # NVMe boot devices
  #
  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf

  #
  # Usb Support
  #
  MdeModulePkg/Bus/Pci/UhciDxe/UhciDxe.inf
  MdeModulePkg/Bus/Pci/EhciDxe/EhciDxe.inf
  MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf {
      <LibraryClasses>
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
      <BuildOptions>
      GCC:*_*_*_CC_FLAGS = -mstrict-align
  }
  MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  MdeModulePkg/Bus/Usb/UsbMouseDxe/UsbMouseDxe.inf
  MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf
  MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
  MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  Drivers/ASIX/Bus/Usb/UsbNetworking/Ax88772c/Ax88772c.inf

  #
  # UEFI application (Shell Embedded Boot Loader)
  #
  ShellPkg/Application/Shell/Shell.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
    <LibraryClasses>
      ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellDriver1CommandsLib/UefiShellDriver1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellAcpiViewCommandLib/UefiShellAcpiViewCommandLib.inf
      NULL|ShellPkg/Library/UefiShellDebug1CommandsLib/UefiShellDebug1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellInstall1CommandsLib/UefiShellInstall1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellNetwork1CommandsLib/UefiShellNetwork1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellNetwork2CommandsLib/UefiShellNetwork2CommandsLib.inf
      OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
      PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
      BcfgCommandLib|ShellPkg/Library/UefiShellBcfgCommandLib/UefiShellBcfgCommandLib.inf
  }
  ShellPkg/DynamicCommand/HttpDynamicCommand/HttpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
  MdeModulePkg/Application/VariableInfo/VariableInfo.inf

  #
  # Default Security Key Enrollment Application.
  #
  SecurityPkg/EnrollFromDefaultKeysApp/EnrollFromDefaultKeysApp.inf

  #
  # Capsule Application
  #
  MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf

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

  #
  # Dynamic Table Factory Dxe
  #
  DynamicTablesPkg/Drivers/DynamicTableFactoryDxe/DynamicTableFactoryDxe.inf {
    <LibraryClasses>
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
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiHmatLibArm/AcpiHmatLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSpmiLibArm/AcpiSpmiLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiTpm2LibArm/AcpiTpm2LibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtCpuTopologyLibArm/SsdtCpuTopologyLibArm.inf
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtPcieLibArm/SsdtPcieLibArm.inf

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

      # AML Fixup
      NULL|DynamicTablesPkg/Library/Acpi/Arm/AcpiSsdtSerialPortLibArm/SsdtSerialPortLibArm.inf
  }

  #
  # Dynamic Tables Manager Dxe
  #
  DynamicTablesPkg/Drivers/DynamicTableManagerDxe/DynamicTableManagerDxe.inf
  #
  # Configuration Manager
  #
  Silicon/NVIDIA/Drivers/ConfigurationManager/ConfigurationManagerDxe.inf

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

  #
  # Secure Boot Support
  #
  SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf

  #
  # Security drivers
  #
  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
  SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf
  SecurityPkg/Pkcs7Verify/Pkcs7VerifyDxe/Pkcs7VerifyDxe.inf

  #
  # RCM Support
  #
  Silicon/NVIDIA/Drivers/RcmDxe/RcmDxe.inf


  #
  # Platform To Driver Configuration Support
  #
  Silicon/NVIDIA/Drivers/PlatformToDriverConfiguration/PlatformToDriverConfiguration.inf


  #
  # Capsule Update Policy Protocol
  #
  Silicon/NVIDIA/Drivers/CapsuleUpdatePolicyDxe/CapsuleUpdatePolicyDxe.inf

  #
  # Tegra EEPROM Support
  #
  Silicon/NVIDIA/Drivers/EepromDxe/Eeprom.inf

  #
  # Third party drivers
  #
  Drivers/Realtek/Bus/Pcie/PcieNetworking/RtkUndiDxe.inf

  #
  # Tegra Pin Control support
  #
  Silicon/NVIDIA/Drivers/TegraPinControlDxe/TegraPinControlDxe.inf

  #
  # Boot Chain Protocol
  #
  Silicon/NVIDIA/Drivers/BootChainDxe/BootChainDxe.inf

  #
  # Ipmi Blob Transfer support
  #
  Silicon/NVIDIA/Drivers/IpmiBlobTransferDxe/IpmiBlobTransferDxe.inf
  Silicon/NVIDIA/Drivers/SmbiosBmcTransfer/SmbiosBmcTransfer.inf


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

[Components.AARCH64]
  #
  # EBC
  #
  MdeModulePkg/Universal/EbcDxe/EbcDxe.inf
