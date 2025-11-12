/** @file
  EDK2 API for OpteeTpmInterfaceFfa

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __OPTEE_TPM_DEVICE_LIB_FFA_H__
#define __OPTEE_TPM_DEVICE_LIB_FFA_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#include <Library/BaseLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/OpteeNvLib.h>

#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>

#define UPPER_32_BITS(n)  ((UINT32)((n) >> 32))
#define LOWER_32_BITS(n)  ((UINT32)(n))

// Define BIT macro for Linux kernel compatibility (if not already defined)
#ifndef BIT
#define BIT(n)  (1UL << (n))
#endif

// FFA Success and Error codes
#define FFA_SUCCESS_AARCH64  0xC4000061
#define FFA_SUCCESS_AARCH32  0x84000061
#define FFA_ERROR_AARCH64    0xC4000060
#define FFA_ERROR_AARCH32    0x84000060
#define FFA_NOT_SUPPORTED    -1

#define FFA_SUCCESS  FFA_SUCCESS_AARCH32
#define FFA_ERROR    FFA_ERROR_AARCH32

#define SPM_MAJOR_VER_MASK   0xFFFF0000
#define SPM_MINOR_VER_MASK   0x0000FFFF
#define SPM_MAJOR_VERSION    1
#define SPM_MAJOR_VER_SHIFT  16

// FFA SMC function IDs
#define ARM_SMC_ID_FFA_VERSION_32                 0x84000063
#define ARM_SMC_ID_FFA_RXTX_UNMAP_32              0x84000067
#define ARM_SMC_ID_FFA_MEM_SHARE_32               0x84000073
#define ARM_SMC_ID_FFA_RXTX_MAP_64                0xC4000066
#define ARM_SMC_ID_FFA_MSG_SEND_DIRECT_REQ_64     0xC400006F
#define ARM_SMC_ID_FFA_MSG_SEND_DIRECT_RESP_64    0xC4000070
#define ARM_SMC_ID_FFA_PARTITION_INFO_GET_REG_64  0xC400008B

// FFA Memory Share flags (FFA v1.1)
#define FFA_MEMORY_SHARE_FLAG_SHARE_MEMORY              0x1
#define FFA_MEMORY_SHARE_FLAG_CLEAR_MEMORY              0x2
#define FFA_MEMORY_SHARE_FLAG_CLEAR_MEMORY_ON_RETRIEVE  0x4
#define FFA_MEMORY_SHARE_FLAG_GRANULE_4K                0x8
#define FFA_MEMORY_SHARE_FLAG_GRANULE_16K               0x10
#define FFA_MEMORY_SHARE_FLAG_GRANULE_64K               0x18

// FFA Memory Share attributes (FFA v1.1)
#define FFA_MEMORY_SHARE_ATTR_NON_SECURE     0x0
#define FFA_MEMORY_SHARE_ATTR_SECURE         0x1
#define FFA_MEMORY_SHARE_ATTR_READ_ONLY      0x2
#define FFA_MEMORY_SHARE_ATTR_READ_WRITE     0x0
#define FFA_MEMORY_SHARE_ATTR_NON_CACHEABLE  0x4
#define FFA_MEMORY_SHARE_ATTR_CACHEABLE      0x8
#define FFA_MEMORY_SHARE_ATTR_SHAREABLE      0x10

// FFA Memory Access Permissions
#define FFA_MEM_RO       BIT(0)         // Read-only
#define FFA_MEM_RW       BIT(1)         // Read-write
#define FFA_MEM_NO_EXEC  BIT(2)         // No execute
#define FFA_MEM_EXEC     BIT(3)         // Execute

// FFA Memory Attributes
#define FFA_MEM_NORMAL  BIT(5)          // Normal memory

// FFA Memory Cacheability Attributes
#define FFA_MEM_WRITE_BACK  (3 << 2)      // Write-back cacheable

// FFA Memory Shareability Attributes
#define FFA_MEM_NON_SHAREABLE    (0)    // Non-shareable
#define FFA_MEM_OUTER_SHAREABLE  (2)    // Outer shareable
#define FFA_MEM_INNER_SHAREABLE  (3)    // Inner shareable

// FFA Memory Region Flags (based on Linux kernel arm_ffa.h)
#define FFA_MEM_CLEAR                 BIT(0)   // Clear memory after unmapping
#define FFA_TIME_SLICE_ENABLE         BIT(1)   // Allow time slicing
#define FFA_MEM_RETRIEVE_TYPE_SHARE   (1 << 3) // Share operation
#define FFA_MEM_RETRIEVE_TYPE_LEND    (2 << 3) // Lend operation
#define FFA_MEM_RETRIEVE_TYPE_DONATE  (3 << 3) // Donate operation

// OP-TEE FFA blocking and yielding call macros
#define OPTEE_FFA_BLOCKING_CALL(id)  (id)
#define OPTEE_FFA_YIELDING_CALL_BIT  31
#define OPTEE_FFA_YIELDING_CALL(id)  ((id) | BIT(OPTEE_FFA_YIELDING_CALL_BIT))
#define OPTEE_FFA_YIELDING_CALL_WITH_ARG  OPTEE_FFA_YIELDING_CALL(0)

#define OPTEE_MESSAGE_ATTRIBUTE_META  0x100
#define OPTEE_LOGIN_PUBLIC            0x0

#define RXTX_BUFFER_SIZE   (4 * 1024)
#define RXTX_PAGE_COUNT    (RXTX_BUFFER_SIZE / EFI_PAGE_SIZE)
#define MAX_RETRIES        3
#define BACKOFF_TIME_USEC  100

//
// UUID struct compliant with RFC4122 (network byte order).
//
typedef struct {
  UINT32    Data1;
  UINT16    Data2;
  UINT16    Data3;
  UINT8     Data4[8];
} RFC4122_UUID;

// FFA Memory Transaction Descriptor (MTD) structures per Linux kernel arm_ffa.h
#pragma pack (1)

/**
  FFA Memory Region Address Range - describes a single contiguous memory region
  Based on Linux kernel struct ffa_mem_region_addr_range
**/
typedef struct {
  UINT64    Address;        // Base IPA of memory region, aligned to 4KB
  UINT32    PageCount;      // Number of 4KB pages in the constituent memory region
  UINT32    Reserved;       // Reserved field (MBZ)
} FFA_MEM_REGION_ADDR_RANGE;

/**
  FFA Composite Memory Region - describes collection of memory regions
  Based on Linux kernel struct ffa_composite_mem_region
**/
typedef struct {
  UINT32    TotalPageCount; // Total number of 4KB pages included in this memory region
  UINT32    AddrRangeCount; // Number of constituents included in this memory region range
  UINT64    Reserved;       // Reserved field (MBZ)
  // Followed by array of FFA_MEM_REGION_ADDR_RANGE constituents
} FFA_COMPOSITE_MEM_REGION;

/**
  FFA Memory Region Attributes - describes access permissions for an endpoint
  Based on Linux kernel struct ffa_mem_region_attributes
**/
typedef struct {
  UINT16    Receiver;        // ID of the VM to which memory is being given or shared
  UINT8     Attrs;           // Permissions with which memory should be mapped
  UINT8     Flag;            // Flags for FFA_MEM_RETRIEVE_REQ/RESP
  UINT32    CompositeOffset; // Offset to ffa_composite_mem_region
  UINT64    Reserved;        // Reserved field (MBZ)
} FFA_MEM_REGION_ATTRIBUTES;

/**
  FFA Memory Region - main memory transaction descriptor
  Based on Linux kernel struct ffa_mem_region
**/
typedef struct {
  UINT16    SenderId;       // ID of the VM/owner which originally sent the memory region
  UINT16    Attributes;     // Memory attributes (cacheability, shareability, etc.)
  UINT32    Flags;          // Flags to control behaviour of the transaction
  UINT64    Handle;         // Globally-unique ID assigned by hypervisor
  UINT64    Tag;            // Implementation defined value associated with receiver
  UINT32    EpMemSize;      // Size of the memory region for the endpoint
  UINT32    EpCount;        // Number of ffa_mem_region_attributes entries
  UINT32    EpMemOffset;    // Offset to the memory region for the endpoint
  UINT32    Reserved[3];    // Reserved fields, MBZ
  // Followed by array of FFA_MEM_REGION_ATTRIBUTES ep_mem_access[]
} FFA_MEM_REGION;

/**
  Complete MTD structure for single endpoint, single region sharing
  Following Linux kernel pattern
**/
typedef struct {
  FFA_MEM_REGION               Header;
  FFA_MEM_REGION_ATTRIBUTES    EndpointAttributes;
  FFA_COMPOSITE_MEM_REGION     CompositeRegion;
  FFA_MEM_REGION_ADDR_RANGE    AddressRange;
} FFA_COMPLETE_MTD;

#pragma pack ()

/**
 * Call an SMC to send an FFA request. This function is similar to
 * ArmCallSmc except that it returns extra GP registers as needed for FFA.
 *
 * @param Args    GP registers to send with the SMC request
 */
VOID
CallFfaSmc (
  IN OUT ARM_SMC_ARGS  *Args
  );

#endif // __OPTEE_TPM_DEVICE_LIB_FFA_H__
