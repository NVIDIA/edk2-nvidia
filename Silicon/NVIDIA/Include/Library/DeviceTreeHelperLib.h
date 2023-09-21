/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __DEVICE_TREE_HELPER_LIB_H__
#define __DEVICE_TREE_HELPER_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/HardwareInterrupt.h>

#define DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET  0x20
#define DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET  0x10
#define DEVICETREE_TO_ACPI_INTERRUPT_NUM(InterruptData) \
  (InterruptData.Interrupt + (InterruptData.Type == INTERRUPT_SPI_TYPE ? \
                                                 DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :\
                                                 DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET))

typedef enum {
  INTERRUPT_SPI_TYPE,
  INTERRUPT_PPI_TYPE,
  INTERRUPT_MAX_TYPE
} NVIDIA_DEVICE_TREE_INTERRUPT_TYPE;

typedef enum {
  INTERRUPT_LO_TO_HI_EDGE = 1,
  INTERRUPT_HI_TO_LO_EDGE = 2,          // Invalid for SPIs
  INTERRUPT_HI_LEVEL      = 4,
  INTERRUPT_LO_LEVEL      = 8           // Invalid for SPIs
} NVIDIA_DEVICE_TREE_INTERRUPT_FLAG;

typedef struct {
  EFI_PHYSICAL_ADDRESS    BaseAddress;
  UINTN                   Size;
  CONST CHAR8             *Name;
} NVIDIA_DEVICE_TREE_REGISTER_DATA;

typedef struct {
  NVIDIA_DEVICE_TREE_INTERRUPT_TYPE    Type;
  HARDWARE_INTERRUPT_SOURCE            Interrupt;
  NVIDIA_DEVICE_TREE_INTERRUPT_FLAG    Flag;
  CONST CHAR8                          *Name;
  CONST CHAR8                          *ControllerCompatible;
} NVIDIA_DEVICE_TREE_INTERRUPT_DATA;

/**
  Set the base address and size of the device tree

  This is to support the use cases when the HOB list is not populated.

  @param  DeviceTree        - Pointer to base Address of the device tree.
  @param  DeviceTreeSize    - Pointer to size of the device tree.

**/
VOID
SetDeviceTreePointer (
  IN  VOID   *DeviceTree,
  IN  UINTN  DeviceTreeSize
  );

/**
  Returns the enabled nodes that match the compatible string

  @param  CompatibleString - String to located devices for
  @param  NodeHandleArray  - Buffer of size NumberOfNodes that will contain the list of supported nodes
  @param  NumberOfNodes    - On input contains size of NodeOffsetArray, on output number of matching nodes.

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfNodes is less than required nodes
  @retval EFI_INVALID_PARAMETER - CompatibleString is NULL
  @retval EFI_INVALID_PARAMETER - NumberOfNodes pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffsetArray is NULL when *NumberOfNodes is not 0
  @retval EFI_NOT_FOUND         - No matching nodes
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetMatchingEnabledDeviceTreeNodes (
  IN CONST CHAR8  *CompatibleString,
  OUT UINT32      *NodeHandleArray OPTIONAL,
  IN OUT UINT32   *NumberOfNodes
  );

/**
  Return kernel and kernel DTB address.

  Look for the /chosen/kernel-start and /chosen/kernel-dtb-start properties. If
  they are set, return them.  These may be set if a kernel was loaded for us.

  @param  KernelStart     - Output the kernel's base address
  @param  KernelDtbStart  - Output the kernel DTB's base address

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_INVALID_PARAMETER - KernelStart is NULL
  @retval EFI_INVALID_PARAMETER - KernelDtbStart is NULL
  @retval EFI_NOT_FOUND         - No matching nodes
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetKernelAddress (
  OUT UINT64  *KernelStart,
  OUT UINT64  *KernelDtbStart
  );

/**
  Returns the specific device tree node information

  @param  NodeHandle      - NodeHandle
  @param  DeviceTreeBase  - Base Address of the device tree.
  @param  NodeOffset      - Offset from DeviceTreeBase to the specified node.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - NodeHandle is in invalid
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeNode (
  IN UINT32  Handle,
  OUT VOID   **DeviceTreeBase,
  OUT INT32  *NodeOffset
  );

/**
  Returns the handle for a specific  node

  @param  DeviceTreeBase  - Base Address of the device tree.
  @param  NodeOffset      - Offset from DeviceTreeBase to the specified node.
  @param  NodeHandle      - NodeHandle

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - NodeHandle is in NULL
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeHandle (
  IN  VOID    *DeviceTreeBase,
  IN  INT32   NodeOffset,
  OUT UINT32  *Handle
  );

/**
  Returns information about the registers of a given device tree node

  @param  NodeHandle        - NodeHandle
  @param  RegisterArray     - Buffer of size NumberOfRegisters that will contain the list of register information
  @param  NumberOfRegisters - On input contains size of RegisterArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfRegisters is less than required registers
  @retval EFI_INVALID_PARAMETER - Handle is invalid
  @retval EFI_INVALID_PARAMETER - NumberOfRegisters pointer is NULL
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL when *NumberOfRegisters is not 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeRegisters (
  IN UINT32                             Handle,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  );

/**
  Gets the offset of the interrupt-parent of the specified node

  @param  [in]  DeviceTreeBase   - Base Address of the device tree.
  @param  [in]  NodeOffset       - Offset from DeviceTreeBase to the specified node.
  @param  [out] ParentNodeOffset - The interrupt parent node offset

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is 0
  @retval EFI_INVALID_PARAMETER - ParentNodeOffset pointer is NULL

**/
EFI_STATUS
EFIAPI
GetInterruptParentOffset (
  IN CONST VOID  *DeviceTree,
  IN INT32       NodeOffset,
  OUT INT32      *ParentNodeOffset
  );

/**
  Returns information about the interrupts of a given device tree node

  @param  [in]      NodeHandle         - NodeHandle
  @param  [out]     InterruptArray     - Buffer of size NumberOfInterrupts that will contain the list of interrupt information
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required registers
  @retval EFI_INVALID_PARAMETER - Handle is invalid
  @retval EFI_INVALID_PARAMETER - NumberOfInterrupts pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptArray is NULL when *NumberOfInterrupts is not 0
  @retval EFI_NOT_FOUND         - No interrupts
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeInterrupts (
  IN UINT32                              Handle,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT UINT32                          *NumberOfInterrupts
  );

#endif //__DEVICE_TREE_HELPER_LIB_H__
