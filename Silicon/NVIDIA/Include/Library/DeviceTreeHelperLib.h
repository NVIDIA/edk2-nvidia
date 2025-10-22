/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#define NVIDIA_DEVICE_TREE_PHANDLE_INVALID  MAX_UINT32
#define DEVICE_ID_INVALID                   MAX_UINT32

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
  EFI_PHYSICAL_ADDRESS    ChildAddressHigh;
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

typedef struct {
  EFI_PHYSICAL_ADDRESS                 ChildAddressLow;
  EFI_PHYSICAL_ADDRESS                 ChildAddressHigh;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA    ChildInterrupt;
  INT32                                InterruptParentPhandle;
  EFI_PHYSICAL_ADDRESS                 ParentAddressLow;
  EFI_PHYSICAL_ADDRESS                 ParentAddressHigh;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA    ParentInterrupt;
} NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA;

typedef struct {
  UINT32    Phandle;
  UINT32    Base;
} NVIDIA_DEVICE_TREE_CONTROLLER_DATA;

typedef struct {
  UINT32                                RidBase;
  NVIDIA_DEVICE_TREE_CONTROLLER_DATA    Controller;
  UINT32                                Length;
} NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA;

typedef enum {
  CACHE_TYPE_UNIFIED = 0, // MPAM wants Type to be 0 for L3 caches
  CACHE_TYPE_ICACHE,
  CACHE_TYPE_DCACHE
} NVIDIA_DEVICE_TREE_CACHE_TYPE;

typedef struct {
  CONST CHAR8    *SizeStr;
  CONST CHAR8    *SetsStr;
  CONST CHAR8    *BlockSizeStr;
  CONST CHAR8    *LineSizeStr;
} NVIDIA_DEVICE_TREE_CACHE_FIELD_STRINGS;

typedef struct {
  NVIDIA_DEVICE_TREE_CACHE_TYPE    Type;
  UINT32                           CacheId;     // This cache's phandle
  UINT32                           CacheLevel;  // 1, 2, or 3
  UINT32                           CacheSize;
  UINT32                           CacheSets;
  UINT32                           CacheBlockSize;
  UINT32                           CacheLineSize;
  UINT32                           NextLevelCache; // Next level's phandle
} NVIDIA_DEVICE_TREE_CACHE_DATA;

typedef struct {
  UINT32    IommuPhandle;
  UINT32    MasterDeviceId;                         // Can be DEVICE_ID_INVALID
  UINT32    DmaWindowStart;
  UINT64    DmaWindowLength;                        // Zero means no DMA window info
} NVIDIA_DEVICE_TREE_IOMMUS_DATA;

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
  Get the named subnode.

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The name property and depth from the starting node is checked.

  @param [in]  NodeName         Name of the Subnode to look for.
  @param [in]  NodeOffset       Node offset to start the search.
                                This first node is skipped.
                                Write (-1) to search the top level.
  @param [out] SubNodeOffset    The offset of the named subnode.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_DEVICE_ERROR        Error getting Device Tree.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNamedSubnode (
  IN      CONST CHAR8  *NodeName,
  IN            INT32  NodeOffset,
  OUT           INT32  *SubNodeOffset
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
  Get the next subnode node with at least one compatible property.

  The status property is checked and if present needs to be "okay"

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
  @param [in]  ParentOffset     Offset of parent node of subnodes to search.
  @param [in, out]  NodeOffset  At entry: 0 to start with first subnode or
                                          subnode offset to continue the search
                                          after moving to next subnode.
                                At exit:  If success, contains the offset of
                                          the next subnode in the branch
                                          being compatible.  May be passed as
                                          NodeOffset in subsequent call to
                                          continue search.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextCompatibleSubnode (
  IN      CONST CHAR8  **CompatibleInfo,
  IN            INT32  ParentOffset,
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
  Returns the parent offset of the specified node

  @param[in] NodeOffset
  @param[out] ParentOffset

  @retval EFI_SUCCESS           - Parent offset returned
  @retval EFI_NOT_FOUND         - Node does not have a parent
  @retval EFI_INVALID_PARAMETER - ParentOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors
**/
EFI_STATUS
EFIAPI
DeviceTreeGetParentOffset (
  IN  INT32  NodeOffset,
  OUT INT32  *ParentOffset
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
  @param[in]      String        - String to match, supports * as wildcard
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
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required entries
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

/**
  Returns information about the interrupt map of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     InterruptMapArray  - Buffer of size NumberOfMaps that will contain the list of interrupt map information
  @param  [in, out] NumberOfMaps       - On input contains size of InterruptMapArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfMaps is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfMaps pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptMapArray is NULL when *NumberOfMaps is not 0
  @retval EFI_NOT_FOUND         - No interrupt maps
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetInterruptMap (
  IN INT32                                   NodeOffset,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA  *InterruptMapArray OPTIONAL,
  IN OUT UINT32                              *NumberOfMaps
  );

/**
  Returns information about the msi or iommu map of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [in]      MapName            - "msi-map" or "iommu-map"
  @param  [out]     MapArray           - Buffer of size NumberOfMaps that will contain the list of map information
  @param  [in, out] NumberOfMaps       - On input contains size of the MapArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfMaps is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfMaps pointer is NULL
  @retval EFI_INVALID_PARAMETER - MapArray is NULL when *NumberOfMaps is not 0
  @retval EFI_NOT_FOUND         - No maps found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetMsiIommuMap (
  IN INT32                                   NodeOffset,
  IN CONST CHAR8                             *MapName,
  OUT NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *MapArray OPTIONAL,
  IN OUT UINT32                              *NumberOfMaps
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

/**
  Returns information about the msi parent a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     Array              - Buffer of size NumberOfParents that will contain the list of msi parent information
  @param  [in, out] NumberOfParents    - On input contains size of the Array, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfParents is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfParents pointer is NULL
  @retval EFI_INVALID_PARAMETER - Array is NULL when *NumberOfParents is not 0
  @retval EFI_NOT_FOUND         - No parents found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetMsiParent (
  IN INT32                                NodeOffset,
  OUT NVIDIA_DEVICE_TREE_CONTROLLER_DATA  *Array OPTIONAL,
  IN OUT UINT32                           *NumberOfParents
  );

/**
  Returns information about the iommus of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     Array              - Buffer of size NumberOfIommus that will contain the list of iommu information
  @param  [in, out] NumberOfIommus     - On input contains size of the Array, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfIommus is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfIommus pointer is NULL
  @retval EFI_INVALID_PARAMETER - Array is NULL when *NumberOfIommus is not 0
  @retval EFI_NOT_FOUND         - No iommus found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetIommus (
  IN INT32                            NodeOffset,
  OUT NVIDIA_DEVICE_TREE_IOMMUS_DATA  *Array OPTIONAL,
  IN OUT UINT32                       *NumberOfIommus
  );

/**
  Returns information about the cache of a given device tree node

  @param  [in]      NodeOffset       - Node offset of the device
  @param  [in, out] CacheData        - Buffer for the cache data. Type field specifies
                                       the type of cache data to populate from the Node

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - CacheData pointer is NULL
  @retval EFI_NOT_FOUND         - No cache data of Type found in the node
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetCacheData (
  IN INT32                              NodeOffset,
  IN OUT NVIDIA_DEVICE_TREE_CACHE_DATA  *CacheData
  );

/**
  Check if a node has a matching compatible property.

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
                                Strings support * as wildcard.
  @param [in]  NodeOffset       Node to check

  @retval EFI_SUCCESS            The node matches one of the compatible strings.
  @retval EFI_NOT_FOUND          Node doesn't match or is disabled.
  @retval Others                 An error occurred.

**/
EFI_STATUS
EFIAPI
DeviceTreeCheckNodeCompatibility (
  IN      CONST CHAR8  **CompatibleInfo,
  IN            INT32  NodeOffset
  );

/**
  Check if a node has a matching compatible property.

  @param [in]  Compatible       Pointer to a compatible string.
                                String support * as wildcard.
  @param [in]  NodeOffset       Node to check

  @retval EFI_SUCCESS            The node matches the compatible string.
  @retval EFI_NOT_FOUND          Node doesn't match or is disabled.
  @retval Others                 An error occurred.

**/
EFI_STATUS
EFIAPI
DeviceTreeCheckNodeSingleCompatibility (
  IN      CONST CHAR8  *Compatible,
  IN            INT32  NodeOffset
  );

/**
  Set the specified property in Node

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[in]      PropertyData  - Data of the property
  @param[in]      PropertySize  - Size of the property node.

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - PropertySize is positive, but PropertyData is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeSetNodeProperty (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  IN  CONST VOID   *PropertyData,
  IN  UINT32       PropertySize
  );

/**
  Finds a register by name in a register array.

  @param[in]  RegisterName      - Name of register to find.
  @param[in]  NumberOfRegisters - Size of RegisterArray.
  @param[in]  RegisterArray     - Buffer of size NumberOfRegisters that contains the register information.
  @param[out] RegisterIndex     - Pointer to save index of register matching RegisterName.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - RegisterIndex pointer is NULL
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL
  @retval EFI_INVALID_PARAMETER - RegisterName is NULL
  @retval EFI_NOT_FOUND         - No register matching RegisterName

**/
EFI_STATUS
EFIAPI
DeviceTreeFindRegisterByName (
  IN CONST CHAR8                             *RegisterName,
  IN CONST NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray,
  IN UINT32                                  NumberOfRegisters,
  OUT UINT32                                 *RegisterIndex
  );

/**
  Returns pointer to name string for node in DTB.  This pointer may become
  invalid if any DTB changes are made.

  @param[in]      NodeOffset      Node offset

  @retval CONST CHAR8*            Pointer to node name in DTB or NULL if error

**/
CONST CHAR8 *
EFIAPI
DeviceTreeGetNodeName (
  IN  INT32  NodeOffset
  );

/**
  Updates information about the registers of a given device tree node.
  Note: Name fields in the RegisterArray may not be valid upon return
  since they point to strings in the DTB.

  @param[in]  NodeOffset        - NodeHandle
  @param[in]  RegisterArray     - Buffer of size NumberOfRegisters that contains the list of register information
  @param[in]  NumberOfRegisters - Contains size of RegisterArray

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL or NumberOfRegisters is 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeSetRegisters (
  IN INT32                                   NodeOffset,
  IN CONST NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray,
  IN UINT32                                  NumberOfRegisters
  );

/**
  Returns unit address for node in DTB

  @param[in]      NodeOffset      Node offset

  @retval         UINT64          Unit address of node
                  0               Address could not be parsed from node name

**/
UINT64
EFIAPI
DeviceTreeGetNodeUnitAddress (
  IN  INT32  NodeOffset
  );

/**
  Check if node is enabled - status property missing or set to "okay"

  @param [in]  NodeOffset       Node offset to check.

  @retval EFI_SUCCESS            The node is enabled.
  @retval EFI_NOT_FOUND          The node is not enabled.

**/
EFI_STATUS
EFIAPI
DeviceTreeNodeIsEnabled (
  IN            INT32  NodeOffset
  );

#endif //__DEVICE_TREE_HELPER_LIB_H__
