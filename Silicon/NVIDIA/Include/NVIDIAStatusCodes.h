/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _NVIDIA_STATUS_CODES_H_
#define _NVIDIA_STATUS_CODES_H_

///
/// A Status Code Value is made up of the class, subclass, and an operation.
///
///@{
#define NV_STATUS_CODE_CLASS_MASK      0xFF000000
#define NV_STATUS_CODE_SUBCLASS_MASK   0x00FF0000
#define NV_STATUS_CODE_OPERATION_MASK  0x0000FFFF
///@}

///
/// Nvidia-specific class definitions
///
///@{
#define EFI_CLASS_NV_HARDWARE  0xC0000000
#define EFI_CLASS_NV_FIRMWARE  0xC1000000
///@}

///
/// Nvidia-hardware Subclass definitions.
///
///@{
#define EFI_NV_HARDWARE_UNSPECIFIED  (EFI_CLASS_NV_HARDWARE | 0x00000000)
#define EFI_NV_HARDWARE_QSPI         (EFI_CLASS_NV_HARDWARE | 0x00010000)
#define EFI_NV_HARDWARE_UPHY         (EFI_CLASS_NV_HARDWARE | 0x00020000)
#define EFI_NV_HARDWARE_NVLINK       (EFI_CLASS_NV_HARDWARE | 0x00030000)
#define EFI_NV_HARDWARE_NVC2C        (EFI_CLASS_NV_HARDWARE | 0x00040000)
#define EFI_NV_HARDWARE_DRAM         (EFI_CLASS_NV_HARDWARE | 0x00050000)
#define EFI_NV_HARDWARE_THERMAL      (EFI_CLASS_NV_HARDWARE | 0x00060000)
#define EFI_NV_HARDWARE_CPU          (EFI_CLASS_NV_HARDWARE | 0x00070000)
///@}

///
/// Nvidia-firmware Subclass definitions.
///
///@{
#define EFI_NV_FIRMWARE_UNSPECIFIED  (EFI_CLASS_NV_FIRMWARE | 0x00000000)
#define EFI_NV_FIRMWARE_BOOT         (EFI_CLASS_NV_FIRMWARE | 0x00010000)
#define EFI_NV_FIRMWARE_MON          (EFI_CLASS_NV_FIRMWARE | 0x00020000)
#define EFI_NV_FIRMWARE_UEFI         (EFI_CLASS_NV_FIRMWARE | 0x00030000)
///@}

///
/// Nvidia Hardware QSPI progress code definitions.
///
///@{
#define EFI_NV_HW_QSPI_PC_FW_FLASH_INIT    (EFI_NV_HARDWARE_QSPI | 0x00000000)
#define EFI_NV_HW_QSPI_PC_DATA_FLASH_INIT  (EFI_NV_HARDWARE_QSPI | 0x00000001)
///@}

///
/// Nvidia Hardware QSPI error code definitions.
///
///@{
#define EFI_NV_HW_QSPI_EC_FW_FLASH_INIT_FAILED    (EFI_NV_HARDWARE_QSPI | 0x00000000)
#define EFI_NV_HW_QSPI_EC_DATA_FLASH_INIT_FAILED  (EFI_NV_HARDWARE_QSPI | 0x00000001)
///@}

///
/// Nvidia Hardware UPHY progress code definitions.
///
///@{
#define EFI_NV_HW_UPHY_PC_INIT  (EFI_NV_HARDWARE_UPHY | 0x00000000)
///@}

///
/// Nvidia Hardware UPHY error code definitions.
///
///@{
#define EFI_NV_HW_UPHY_EC_INIT_FAILED  (EFI_NV_HARDWARE_UPHY | 0x00000000)
///@}

///
/// Nvidia Hardware NVLINK progress code definitions.
///
///@{
#define EFI_NV_HW_NVLINK_PC_INIT           (EFI_NV_HARDWARE_NVLINK | 0x00000000)
#define EFI_NV_HW_NVLINK_PC_TRAINING_DONE  (EFI_NV_HARDWARE_NVLINK | 0x00000001)
///@}

///
/// Nvidia Hardware NVLINK error code definitions.
///
///@{
#define EFI_NV_HW_NVLINK_EC_INIT_FAILED       (EFI_NV_HARDWARE_NVLINK | 0x00000000)
#define EFI_NV_HW_NVLINK_EC_TRAINING_TIMEOUT  (EFI_NV_HARDWARE_NVLINK | 0x00000001)
///@}

///
/// Nvidia Hardware NVC2C progress code definitions.
///
///@{
#define EFI_NV_HW_NVC2C_PC_INIT           (EFI_NV_HARDWARE_NVC2C | 0x00000000)
#define EFI_NV_HW_NVC2C_PC_TRAINING_DONE  (EFI_NV_HARDWARE_NVC2C | 0x00000001)
///@}

///
/// Nvidia Hardware NVC2C error code definitions.
///
///@{
#define EFI_NV_HW_NVC2C_EC_INIT_FAILED       (EFI_NV_HARDWARE_NVC2C | 0x00000000)
#define EFI_NV_HW_NVC2C_EC_TRAINING_TIMEOUT  (EFI_NV_HARDWARE_NVC2C | 0x00000001)
///@}

///
/// Nvidia Hardware THERMAL error code definitions.
///
///@{
#define EFI_NV_HW_THERMAL_EC_TEMP_OUT_OF_RANGE  (EFI_NV_HARDWARE_THERMAL | 0x00000000)
///@}

///
/// Nvidia Firmware Boot progress code definitions.
///
///@{
#define EFI_NV_FW_BOOT_PC_BRBCT_LOAD            (EFI_NV_FIRMWARE_BOOT | 0x00000000)
#define EFI_NV_FW_BOOT_PC_BOOT_CHAIN_SELECT     (EFI_NV_FIRMWARE_BOOT | 0x00000001)
#define EFI_NV_FW_BOOT_PC_BR_EXIT_DONE          (EFI_NV_FIRMWARE_BOOT | 0x00000002)
#define EFI_NV_FW_BOOT_PC_USBRCM_MODE           (EFI_NV_FIRMWARE_BOOT | 0x00000003)
#define EFI_NV_FW_BOOT_PC_HALT_EXCLUDED_SOCKET  (EFI_NV_FIRMWARE_BOOT | 0x00000004)
#define EFI_NV_FW_BOOT_PC_MB1_START             (EFI_NV_FIRMWARE_BOOT | 0x00000005)
#define EFI_NV_FW_BOOT_PC_CSA_COMPLETE          (EFI_NV_FIRMWARE_BOOT | 0x00000006)
///@}

///
/// Nvidia Firmware Boot error code definitions.
///
///@{
#define EFI_NV_FW_BOOT_EC_BRBCT_LOAD_FAILED   (EFI_NV_FIRMWARE_BOOT | 0x00000000)
#define EFI_NV_FW_BOOT_EC_BINARY_LOAD_FAILED  (EFI_NV_FIRMWARE_BOOT | 0x00000001)
#define EFI_NV_FW_BOOT_EC_CSA_FAILED          (EFI_NV_FIRMWARE_BOOT | 0x00000002)
///@}

///
/// Nvidia Firmware Boot debug code definitions.
///
///@{
#define EFI_NV_FW_BOOT_DC_BINARY_LOAD  (EFI_NV_FIRMWARE_BOOT | 0x00000000)
#define EFI_NV_FW_BOOT_DC_CSA_START    (EFI_NV_FIRMWARE_BOOT | 0x00000001)
///@}

///
/// Nvidia Firmware UEFI error code definitions.
///
///@{
#define EFI_NV_FW_UEFI_EC_NO_SMBIOS_TABLE         (EFI_NV_FIRMWARE_UEFI | 0x00000000)
#define EFI_NV_FW_UEFI_EC_SMBIOS_TRANSFER_FAILED  (EFI_NV_FIRMWARE_UEFI | 0x00000001)
///@}

#endif
