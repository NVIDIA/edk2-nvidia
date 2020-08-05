#
#  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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


################################################################################
#
# FD Section
# The [FD] Section is made up of the definition statements and a
# description of what goes into  the Flash Device Image.  Each FD section
# defines one flash "device" image.  A flash device image may be one of
# the following: Removable media bootable image (like a boot floppy
# image,) an Option ROM image (that would be "flashed" into an add-in
# card,) a System "Flash"  image (that would be burned into a system's
# flash) or an Update ("Capsule") image that will be used to update and
# existing system flash.
#
################################################################################

[FD.UEFI_VARIABLES]
BaseAddress   = 0x00000000
Size          = 0x00010000
ErasePolarity = 1

# This one is tricky, it must be: BlockSize * NumBlocks = Size
BlockSize     = 0x00000200
NumBlocks     = 0x80

################################################################################
#
# Following are lists of FD Region layout which correspond to the locations of different
# images within the flash device.
#
# Regions must be defined in ascending order and may not overlap.
#
# A Layout Region start with a eight digit hex offset (leading "0x" required) followed by
# the pipe "|" character, followed by the size of the region, also in hex with the leading
# "0x" characters. Like:
# Offset|Size
# PcdOffsetCName|PcdSizeCName
# RegionType <FV, DATA, or FILE>
#
################################################################################

0x00000000|0x00010000
gArmTokenSpaceGuid.PcdFvBaseAddress|gArmTokenSpaceGuid.PcdFvSize

DATA = {
  # EFI_FIRMWARE_VOLUME_HEADER
  # ZeroVector
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  # FileSystemGuid: gEfiSystemNvDataFvGuid =
  # { 0xFFF12B8D, 0x7696, 0x4C8B, { 0xA9, 0x85, 0x27, 0x47, 0x07, 0x5B, 0x4F, 0x50 }}
  0x8D, 0x2B, 0xF1, 0xFF, 0x96, 0x76, 0x8B, 0x4C,
  0xA9, 0x85, 0x27, 0x47, 0x07, 0x5B, 0x4F, 0x50,
  # FvLength: 0x10000
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  # Signature "_FVH"
  0x5F, 0x46, 0x56, 0x48,
  # Attributes
  0x36, 0x0E, 0x00, 0x00,
  # HeaderLength
  0x48, 0x00,
  # CheckSum
  0x87, 0xF7,
  # ExtHeaderOffset
  0x00, 0x00,
  #Reserved
  0x00,
  #Revision
  0x02,
  # Blockmap[0]: 0x80 Blocks * 0x200 Bytes / Block
  0x80, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  # Blockmap[1]: End
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  # VARIABLE_STORE_HEADER
  # Signature: gEfiAuthenticatedVariableGuid =
  # { 0xAAF32C78, 0x947B, 0x439A, { 0xA1, 0x80, 0x2E, 0x14, 0x4E, 0xC3, 0x77, 0x92 }}
  0x78, 0x2C, 0xF3, 0xAA, 0x7B, 0x94, 0x9A, 0x43,
  0xA1, 0x80, 0x2E, 0x14, 0x4E, 0xC3, 0x77, 0x92,
  # Size: 0x10000 (gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize) -
  #         0x48 (size of EFI_FIRMWARE_VOLUME_HEADER) = 0xFFB8
  # This can speed up the Variable Dispatch a bit.
  0xB8, 0xFF, 0x00, 0x00,
  # FORMATTED: 0x5A
  0x5A,
  #HEALTHY: 0xFE
  0xFE,
  #Reserved: UINT16
  0x00, 0x00,
  #Reserved1: UINT32
  0x00, 0x00, 0x00, 0x00,

  # Add DtAcpiPref Variable & Force ACPI Boot.
  # VARIABLE_HEADER
  # StartId: 0x55AA
  0xAA, 0x55,
  # State: 0x3F
  0x3F,
  # Reserved: UINT8
  0x00,
  # Attributes: VARIABLE_ATTRIBUTE_NV_BS
  0x03, 0x00, 0x00, 0x00,
  # MonotonicCount:
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  # TimeStamp:
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  # PubKeyIndex:
  0x00, 0x00, 0x00, 0x00,
  # NameSize:
  0x16, 0x00, 0x00, 0x00,
  # DataSize:
  0x04, 0x00, 0x00, 0x00,
  # VendorGuid: gDtPlatformFormSetGuid =
  # { 0x2B7A240D, 0xD5AD, 0x4FD6, { 0xBE, 0x1C, 0xDF, 0xA4, 0x41, 0x5F, 0x55, 0x26 }}
  0x0D, 0x24, 0x7A, 0x2B, 0xAD, 0xD5, 0xD6, 0x4F,
  0xBE, 0x1C, 0xDF, 0xA4, 0x41, 0x5F, 0x55, 0x26,
  # VariableName: DtAcpiPref
  0x44, 0x00, 0x74, 0x00, 0x41, 0x00, 0x63, 0x00,
  0x70, 0x00, 0x69, 0x00, 0x50, 0x00, 0x72, 0x00,
  0x65, 0x00, 0x66, 0x00, 0x00, 0x00,
  # VariableData: DT_ACPI_SELECT_ACPI
  0x01, 0x00, 0x00, 0x00
}
