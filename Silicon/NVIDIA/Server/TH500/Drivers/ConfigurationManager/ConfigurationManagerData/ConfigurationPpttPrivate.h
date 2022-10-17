/** @file
  Configuration Manager Library Private header file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATIONMANAGERLIBPRIVATE_H_
#define __CONFIGURATIONMANAGERLIBPRIVATE_H_

#define PLATFORM_MAX_SOCKETS            (PcdGet32 (PcdTegraMaxSockets))
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_CPUS               (PLATFORM_MAX_CLUSTERS * \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)
#define PLATFORM_MAX_CORES_PER_SOCKET   (PLATFORM_MAX_CPUS / PLATFORM_MAX_SOCKETS)

#define CACHE_TYPE_UNIFIED  0
#define CACHE_TYPE_ICACHE   1
#define CACHE_TYPE_DCACHE   2
#define GET_CACHE_ID(Level, Type, Cluster, Core, Socket)  ((((3 - (Level)) << 24)) | (((Type) << 16)) | (((Core) << 12)) |(((Cluster) << 8)) | ((Socket) + 1))

// CACHE_NODE structure definition.
#define CACHE_NODE_SIGNATURE  SIGNATURE_32('C','H','N','D')
typedef struct {
  CM_ARM_CACHE_INFO    *CachePtr;
  UINT32               PHandle;
  UINT32               Signature;
  LIST_ENTRY           Link;
} CACHE_NODE;
#define CACHE_NODE_FROM_LINK(a) \
        CR(a, CACHE_NODE, Link, CACHE_NODE_SIGNATURE)

#endif /* __CONFIGURATIONMANAGERLIBPRIVATE_H_ */
