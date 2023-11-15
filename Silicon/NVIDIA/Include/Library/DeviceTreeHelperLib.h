/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#define NVIDIA_DEVICE_TREE_COMPATIBLE_MAX_STRING_LEN  32

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
  EFI_PHYSICAL_ADDRESS    ChildAddress;
  EFI_PHYSICAL_ADDRESS    ParentAddress;
  UINTN                   Size;
  CONST CHAR8             *Name;
} NVIDIA_DEVICE_TREE_RANGES_DATA;

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

  @param[in]  DeviceTree        - Pointer to base Address of the device tree.
  @param[in]  DeviceTreeSize    - Pointer to size of the device tree.

**/
VOID
EFIAPI
SetDeviceTreePointer (
  IN  VOID   *DeviceTree,
  IN  UINTN  DeviceTreeSize
  );

/**
  Get the base address and size of the device tree

  @param[out]  DeviceTree        - Pointer to base Address of the device tree.
  @param[out]  DeviceTreeSize    - Pointer to size of the device tree.

  @retval EFI_SUCCESS           - DeviceTree pointer located
  @retval EFI_INVALID_PARAMETER - DeviceTree is NULL
  @retval EFI_NOT_FOUND         - DeviceTree is not found
**/
EFI_STATUS
EFIAPI
GetDeviceTreePointer (
  OUT  VOID   **DeviceTree,
  OUT  UINTN  *DeviceTreeSize OPTIONAL
  );

/**
  Get the next node with at least one compatible property.

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The status property is checked and if present needs to be "okay"

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextCompatibleNode (
  IN      CONST CHAR8  **CompatibleInfo,
  IN OUT        INT32  *NodeOffset
  );

/**
  Get the count of nodes with at least one compatible property.

  The status property is checked and if present needs to be "okay"

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetCompatibleNodeCount (
  IN      CONST CHAR8  **CompatibleInfo,
  OUT UINT32           *NodeCount
  );

/**
  Get the next node of device_type = "cpu".

  The status property is checked and if present needs to be "okay"

  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextCpuNode (
  IN OUT        INT32  *NodeOffset
  );

/**
  Get the count of nodes with device_type = "cpu"

  The status property is checked and if present needs to be "okay"

  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetCpuNodeCount (
  OUT UINT32  *NodeCount
  );

/**
  Get the next node of device_type = "memory".

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The status property is checked and if present needs to be "okay"

  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextMemoryNode (
  IN OUT        INT32  *NodeOffset
  );

/**
  Get the count of nodes with device_type = "memory"

  The status property is checked and if present needs to be "okay"

  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetMemoryNodeCount (
  OUT UINT32  *NodeCount
  );

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Returns the enabled nodes that match the compatible string

  The handle in this API is not the handle in dtb.

  FUNCTION IS DEPRECATED

  @param  CompatibleString - String to located devices for
  @param  NodeHandleArray  - Buffer of size NumberOfNodes that will contain the list of supported nodes
  @param  NumberOfNodes    - On input contains size of NodeHandleArray, on output number of matching nodes.

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

#endif

/**
  Returns the node with the specified phandle

  @param[in]  NodePHandle - DTB PHandle to search for
  @param[out] NodeOffset  - Node offset of the matching node

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_NOT_FOUND         - Node not found
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPHandle (
  IN UINT32  NodePHandle,
  OUT INT32  *NodeOffset
  );

/**
  Returns the specified node's phandle

  @param[in]  NodeOffset  - Node offset
  @param[out] NodePHandle - DTB PHandle of the node

  @retval EFI_SUCCESS           - PHandle returned
  @retval EFI_NOT_FOUND         - Node does not have a phandle
  @retval EFI_INVALID_PARAMETER - NodePHandle is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePHandle (
  IN  INT32   NodeOffset,
  OUT UINT32  *NodePHandle
  );

/**
  Returns the node with the specified path

  @param[in]  NodePath    - Path to the node
  @param[out] NodeOffset  - Node offset of the matching node

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_NOT_FOUND         - Node not found
  @retval EFI_INVALID_PARAMETER - NodePath is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPath (
  IN CONST CHAR8  *NodePath,
  OUT INT32       *NodeOffset
  );

/**
  Returns the specified node's path

  @param[in]      NodeOffset    - Node offset
  @param[out]     NodePath      - A pointer to a buffer allocated by this function with
                                  AllocatePool() to store the path. If this function
                                  returns EFI_SUCCESS, it stores the path the caller
                                  wants to get. The caller should release the string
                                  buffer with FreePool() after the path is not used any more.
  @param[out]     NodePathSize  - On output size of the path string.

  @retval EFI_SUCCESS           - Path returned
  @retval EFI_INVALID_PARAMETER - NodePath is NULL
  @retval EFI_OUT_OF_RESOURCES  - There are not enough resources to allocate the return buffer NodePath.
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePath (
  IN  INT32   NodeOffset,
  OUT CHAR8   **NodePath,
  OUT UINT32  *NodePathSize OPTIONAL
  );

/**
  Returns the specified property data

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyData  - Data of the property
  @param[in,out]  PropertySize  - Size of the property node.

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeProperty (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT CONST VOID   **PropertyData OPTIONAL,
  OUT UINT32       *PropertySize OPTIONAL
  );

/**
  Returns the uint64 value of the property

  Uses the size of the acutual property node to read the data and converts
  endian to system type from stored big endian

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyValue - Value of the property

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_BAD_BUFFER_SIZE   - Property did not have a size that could be
                                  converted to UINT64
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - PropertyValue is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue64 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT64       *PropertyValue
  );

/**
  Returns the uint32 value of the property

  Uses the size of the acutual property node to read the data and converts
  endian to system type from stored big endian

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyValue - Value of the property

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_BAD_BUFFER_SIZE   - Property did not have a size that could be
                                  converted to UINT32
  @retval EFI_NO_MAPPING        - Value was stored as a 64 bit in DTB but is
                                  greated than MAX_UINT32

  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - PropertyValue is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue32 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT32       *PropertyValue
  );

/**
  Locates the matching string in a string list property.

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[in]      String        - String to match
  @param[out]     Index         - Index of the string to match

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NO_MAPPING        - Property is not present in node
  @retval EFI_NOT_FOUND         - String is not found in the property string list
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - String is NULL
  @retval EFI_INVALID_PARAMETER - Index is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeLocateStringIndex (
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Property,
  IN CONST CHAR8  *String,
  OUT UINT32      *Index
  );

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Return kernel and kernel DTB address.

  Look for the /chosen/kernel-start and /chosen/kernel-dtb-start properties. If
  they are set, return them.  These may be set if a kernel was loaded for us.

  FUNCTION IS DEPRECATED

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

  FUNCTION IS DEPRECATED

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

  FUNCTION IS DEPRECATED

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

#endif //DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Returns information about the registers of a given device tree node

  @param[in]  NodeOffset        - NodeHandle
  @param[out] RegisterArray     - Buffer of size NumberOfRegisters that will contain the list of register information
  @param[in]  NumberOfRegisters - On input contains size of RegisterArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfRegisters is less than required registers
  @retval EFI_INVALID_PARAMETER - NumberOfRegisters pointer is NULL
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL when *NumberOfRegisters is not 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetRegisters (
  IN INT32                              NodeOffset,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  );

/**
Returns information about the ranges of a given device tree node

@param[in]      NodeOffset      - NodeHandle
@param[in]      RangeName       - Name of the ranges property ("ranges", "dma-ranges", etc)
@param[out]     RangesArray     - Buffer of size NumberOfRanges that will contain the list of ranges information
@param[in,out]  NumberOfRanges  - On input contains size of RangesArray, on output number of required ranges.

@retval EFI_SUCCESS           - Operation successful
@retval EFI_BUFFER_TOO_SMALL  - NumberOfRanges is less than required ranges
@retval EFI_INVALID_PARAMETER - NumberOfRanges pointer is NULL
@retval EFI_INVALID_PARAMETER - RangesArray is NULL when *NumberOfRanges is not 0
@retval EFI_NOT_FOUND         - No ranges
@retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetRanges (
  IN INT32                            NodeOffset,
  IN CONST CHAR8                      *RangeName,
  OUT NVIDIA_DEVICE_TREE_RANGES_DATA  *RangesArray OPTIONAL,
  IN OUT UINT32                       *NumberOfRanges
  );

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Returns information about the registers of a given device tree node

  FUNCTION IS DEPRECATED

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

#endif //DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Returns information about the interrupts of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     InterruptArray     - Buffer of size NumberOfInterrupts that will contain the list of interrupt information
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required registers
  @retval EFI_INVALID_PARAMETER - NumberOfInterrupts pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptArray is NULL when *NumberOfInterrupts is not 0
  @retval EFI_NOT_FOUND         - No interrupts
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetInterrupts (
  IN INT32                               NodeOffset,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT UINT32                          *NumberOfInterrupts
  );

#ifndef DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

/**
  Gets the offset of the interrupt-parent of the specified node

  FUNCTION IS DEPRECATED

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

  FUNCTION IS DEPRECATED

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

#endif //DISABLE_DEVICETREE_HELPER_DEPRECATED_APIS

#endif //__DEVICE_TREE_HELPER_LIB_H__
